#include "SOPAssign.h"
#include "AttrEvalCallbacks.h"
#include "PrimitiveClassifier.h"
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

	if (lockInputs(context) >= UT_ERROR_ABORT) {
		LOG_DBG << "lockInputs error";
		return error();
	}
	duplicateSource(0, context);

	if (error() < UT_ERROR_ABORT && cookInputGroups(context) < UT_ERROR_ABORT) {
		UT_AutoInterrupt progress("Assigning CityEngine rule attributes...");

		PrimitiveClassifier primCls(this, gdp, context);
		mShapeConverter->getMainAttributes(this, context);

		ShapeData shapeData;
		mShapeConverter->get(gdp, primCls, shapeData, mPRTCtx);
		const bool canContinue = evaluateDefaultRuleAttributes(shapeData, mShapeConverter, mPRTCtx);
		if (!canContinue) {
			LOG_ERR << getName() << ": aborting, could not successfully evaluate default rule attributes";
			return UT_ERROR_ABORT;
		}
		mShapeConverter->put(gdp, primCls, shapeData);
	}

	unlockInputs();

	return error();
}
