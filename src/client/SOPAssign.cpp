#include "SOPAssign.h"
#include "AttrEvalCallbacks.h"
#include "ShapeGenerator.h"
#include "ModelConverter.h"
#include "NodeParameter.h"
#include "LogHandler.h"
#include "MultiWatch.h"

#include "prt/API.h"

#include "UT/UT_Interrupt.h"

#include "boost/algorithm/string.hpp"


namespace {

constexpr bool           DBG                     = false;
constexpr const wchar_t* ENCODER_ID_CGA_EVALATTR = L"com.esri.prt.core.AttributeEvalEncoder";

AttributeMapUPtr getValidEncoderInfo(const wchar_t* encID) {
	const EncoderInfoUPtr encInfo(prt::createEncoderInfo(encID));
	const prt::AttributeMap* encOpts = nullptr;
	encInfo->createValidatedOptionsAndStates(nullptr, &encOpts);
	return AttributeMapUPtr(encOpts);
}

bool evaluateDefaultRuleAttributes(
		ShapeData& shapeData,
		const ShapeConverterUPtr& shapeConverter,
		const PRTContextUPtr& prtCtx
) {
	WA("all");

	assert(shapeData.isValid());

	// setup encoder options for attribute evaluation encoder
	constexpr const wchar_t* encs[] = { ENCODER_ID_CGA_EVALATTR };
	constexpr size_t encsCount = sizeof(encs) / sizeof(encs[0]);
	const AttributeMapUPtr encOpts = getValidEncoderInfo(ENCODER_ID_CGA_EVALATTR);
	const prt::AttributeMap* encsOpts[] = { encOpts.get() };

	// try to get a resolve map
	const ResolveMapUPtr& resolveMap = prtCtx->getResolveMap(shapeConverter->mRPK);
	if (!resolveMap) {
		LOG_WRN << "could not create resolve map from rpk " << shapeConverter->mRPK << ", aborting default rule attribute evaluation";
		return false;
	}

	// create initial shapes
	uint32_t isIdx = 0;
	shapeData.mInitialShapes.reserve(shapeData.mInitialShapeBuilders.size());
	for (auto& isb: shapeData.mInitialShapeBuilders) {
		const std::wstring shapeName = L"shape_" + std::to_wstring(isIdx++);
		if (DBG) LOG_DBG << "evaluating attrs for shape: " << shapeName;

		// persist rule attributes even if empty (need to live until prt::generate is done)
		AttributeMapBuilderUPtr amb(prt::AttributeMapBuilder::create());
		AttributeMapUPtr ruleAttr(amb->createAttributeMap());
		isb->setAttributes(
				shapeConverter->mRuleFile.c_str(),
				shapeConverter->mStartRule.c_str(),
				shapeConverter->mSeed,
				shapeName.c_str(),
				ruleAttr.get(),
				resolveMap.get());

		prt::Status status = prt::STATUS_UNSPECIFIED_ERROR;
		const prt::InitialShape* initialShape = isb->createInitialShapeAndReset(&status);
		if (status == prt::STATUS_OK && initialShape != nullptr) {
			shapeData.mInitialShapes.emplace_back(initialShape);
			shapeData.mRuleAttributeBuilders.emplace_back(std::move(amb));
			shapeData.mRuleAttributes.emplace_back(std::move(ruleAttr));
		}
		else
			LOG_WRN << "failed to create initial shape " << shapeName << ": " << prt::getStatusDescription(status);
	}

	assert(shapeData.isValid());

	// run generate to evaluate default rule attributes
	const RuleFileInfoUPtr ruleFileInfo(shapeConverter->getRuleFileInfo(resolveMap, prtCtx->mPRTCache.get()));
	AttrEvalCallbacks aec(shapeData.mRuleAttributeBuilders, ruleFileInfo);
	const prt::Status stat = prt::generate(shapeData.mInitialShapes.data(), shapeData.mInitialShapes.size(),
	                                       nullptr, encs, encsCount, encsOpts, &aec, prtCtx->mPRTCache.get(),
	                                       nullptr, nullptr, nullptr);
	if (stat != prt::STATUS_OK) {
		LOG_ERR << "assign: prt::generate() failed with status: '" << prt::getStatusDescription(stat) << "' (" << stat << ")";
	}

	assert(shapeData.isValid());

	return true;
}

} // namespace


SOPAssign::SOPAssign(const PRTContextUPtr& pCtx, OP_Network* net, const char* name, OP_Operator* op)
: SOP_Node(net, name, op), mPRTCtx(pCtx), mShapeConverter(new ShapeConverter()) { }

OP_ERROR SOPAssign::cookMySop(OP_Context& context) {
	WA_NEW_LAP
	WA("all");

	evaluatePrimitiveClassifier(context);
	evaluateSharedShapeData(context);

	if (lockInputs(context) >= UT_ERROR_ABORT) {
		LOG_DBG << "lockInputs error";
		return error();
	}
	duplicateSource(0, context);

	if (error() < UT_ERROR_ABORT && cookInputGroups(context) < UT_ERROR_ABORT) {
		UT_AutoInterrupt progress("Assigning CityEngine rule attributes...");

		ShapeData shapeData;
		mShapeConverter->get(gdp, mPrimCls, shapeData, mPRTCtx);
		const bool canContinue = evaluateDefaultRuleAttributes(shapeData, mShapeConverter, mPRTCtx);
		if (!canContinue) {
			LOG_ERR << getName() << ": aborting, could not successfully evaluate default rule attributes";
			return UT_ERROR_ABORT;
		}
		mShapeConverter->put(gdp, mPrimCls, shapeData);
	}

	unlockInputs();

	return error();
}

void SOPAssign::evaluatePrimitiveClassifier(OP_Context &context) {
	const fpreal now = context.getTime();

	// -- primitive classifier attr name
	UT_String utNextClsAttrName;
	evalString(utNextClsAttrName, AssignNodeParams::PARAM_NAME_PRIM_CLS_ATTR.getToken(), 0, now);
	if (utNextClsAttrName.length() > 0)
		mPrimCls.name.adopt(utNextClsAttrName);
	else
		setString(mPrimCls.name, CH_STRING_LITERAL, AssignNodeParams::PARAM_NAME_PRIM_CLS_ATTR.getToken(), 0, now);

	// -- primitive classifier attr type
	const auto typeChoice = evalInt(AssignNodeParams::PARAM_NAME_PRIM_CLS_TYPE.getToken(), 0, now);
	switch (typeChoice) {
		case 0: mPrimCls.type = GA_STORECLASS_STRING; break;
		case 1: mPrimCls.type = GA_STORECLASS_INT;    break;
		case 2: mPrimCls.type = GA_STORECLASS_FLOAT;  break;
		default: break;
	}
}

void SOPAssign::evaluateSharedShapeData(OP_Context &context) {
	WA("all");

	const fpreal now = context.getTime();

	// -- rule package path
	mShapeConverter->mRPK = [this,now](){
		UT_String utNextRPKStr;
		evalString(utNextRPKStr, AssignNodeParams::RPK.getToken(), 0, now);
		return boost::filesystem::path(utNextRPKStr.toStdString());
	}();

	// -- rule file
	mShapeConverter->mRuleFile = [this,now](){
		UT_String utRuleFile;
		evalString(utRuleFile, AssignNodeParams::RULE_FILE.getToken(), 0, now);
		return toUTF16FromOSNarrow(utRuleFile.toStdString());
	}();

	// -- rule style
	mShapeConverter->mStyle = [this,now](){
		UT_String utStyle;
		evalString(utStyle, AssignNodeParams::STYLE.getToken(), 0, now);
		return toUTF16FromOSNarrow(utStyle.toStdString());
	}();

	// -- start rule
	mShapeConverter->mStartRule = [this,now](){
		UT_String utStartRule;
		evalString(utStartRule, AssignNodeParams::START_RULE.getToken(), 0, now);
		return toUTF16FromOSNarrow(utStartRule.toStdString());
	}();

	// -- random seed
	mShapeConverter->mSeed = evalInt(AssignNodeParams::SEED.getToken(), 0, now);
}
