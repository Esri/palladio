#include "SOPAssign.h"
#include "AttrEvalCallbacks.h"
#include "ShapeGenerator.h"
#include "ModelConverter.h"
#include "NodeParameter.h"

#include "prt/API.h"

#include "UT/UT_Interrupt.h"

#include "boost/algorithm/string.hpp"


namespace {

constexpr bool           DBG                       = false;
constexpr const wchar_t* ENCODER_ID_CGA_EVALATTR   = L"com.esri.prt.core.AttributeEvalEncoder";

void evaluateDefaultRuleAttributes(
		ShapeData& shapeData,
		const ShapeConverterUPtr& shapeConverter,
		const PRTContextUPtr& prtCtx
) {
	// setup encoder options for attribute evaluation encoder
	const EncoderInfoUPtr encInfo(prt::createEncoderInfo(ENCODER_ID_CGA_EVALATTR));
	const prt::AttributeMap* encOpts = nullptr;
	encInfo->createValidatedOptionsAndStates(nullptr, &encOpts); // TODO: move into uptr
	constexpr const wchar_t* encs[] = { ENCODER_ID_CGA_EVALATTR };
	constexpr size_t encsCount = sizeof(encs) / sizeof(encs[0]);
	const prt::AttributeMap* encsOpts[] = { encOpts };

	// try to get a resolve map, might be empty (nullptr)
	const ResolveMapUPtr& resolveMap = prtCtx->getResolveMap(shapeConverter->mRPK);

	// create initial shapes
	InitialShapeNOPtrVector iss;
	iss.reserve(shapeData.mInitialShapeBuilders.size());
	uint32_t isIdx = 0;
	for (auto& isb: shapeData.mInitialShapeBuilders) {
		const std::wstring shapeName = L"shape_" + std::to_wstring(isIdx++);

		// persist rule attributes even if empty (need to live until prt::generate is done)
		// TODO: switch to shared ptrs and avoid all these copies
		shapeData.mRuleAttributeBuilders.emplace_back(prt::AttributeMapBuilder::create());
		shapeData.mRuleAttributes.emplace_back(shapeData.mRuleAttributeBuilders.back()->createAttributeMap());

		isb->setAttributes(
				shapeConverter->mRuleFile.c_str(),
				shapeConverter->mStartRule.c_str(),
				shapeConverter->mSeed,
				shapeName.c_str(),
				shapeData.mRuleAttributes.back().get(),
				resolveMap.get());

		prt::Status status = prt::STATUS_UNSPECIFIED_ERROR;
		const prt::InitialShape* initialShape = isb->createInitialShapeAndReset(&status);
		if (status != prt::STATUS_OK) {
			LOG_WRN << "failed to create initial shape: " << prt::getStatusDescription(status);
			continue;
		}
		iss.emplace_back(initialShape);
	}

	// run generate to evaluate default rule attributes
	const RuleFileInfoUPtr ruleFileInfo(shapeConverter->getRuleFileInfo(resolveMap, prtCtx->mPRTCache.get()));
	AttrEvalCallbacks aec(shapeData.mRuleAttributeBuilders, ruleFileInfo);
	// TODO: should we run this in parallel batches as well? much to gain?
	const prt::Status stat = prt::generate(iss.data(), iss.size(), nullptr, encs, encsCount, encsOpts, &aec, prtCtx->mPRTCache.get(), nullptr, nullptr, nullptr);
	if (stat != prt::STATUS_OK) {
		LOG_ERR << "assign: prt::generate() failed with status: '" << prt::getStatusDescription(stat) << "' (" << stat << ")";
	}

	if (encOpts)
		encOpts->destroy();
}

} // namespace


SOPAssign::SOPAssign(const PRTContextUPtr& pCtx, OP_Network* net, const char* name, OP_Operator* op)
: SOP_Node(net, name, op), mPRTCtx(pCtx), mShapeConverter(new ShapeConverter()) { }

OP_ERROR SOPAssign::cookMySop(OP_Context& context) {
	std::chrono::time_point<std::chrono::system_clock> start, end;

	if (!updateSharedShapeDataFromParams(context))
		return UT_ERROR_ABORT;

	if (lockInputs(context) >= UT_ERROR_ABORT) {
		LOG_DBG << "lockInputs error";
		return error();
	}
	duplicateSource(0, context); // TODO: is this actually needed (we only set prim attrs)?

	if (error() < UT_ERROR_ABORT && cookInputGroups(context) < UT_ERROR_ABORT) {
		UT_AutoInterrupt progress("Assigning CityEngine rule attributes...");

		ShapeData shapeData;

		start = std::chrono::system_clock::now();
		mShapeConverter->get(gdp, shapeData, mPRTCtx);
		end = std::chrono::system_clock::now();
		LOG_INF << getName() << ": extracting shapes from detail: " << std::chrono::duration<double>(end - start).count() << "s\n";

		start = std::chrono::system_clock::now();
		evaluateDefaultRuleAttributes(shapeData, mShapeConverter, mPRTCtx);
		end = std::chrono::system_clock::now();
		LOG_INF << getName() << ": evaluating default rule attributes on shapes: " << std::chrono::duration<double>(end - start).count() << "s\n";

		start = std::chrono::system_clock::now();
		mShapeConverter->put(gdp, shapeData);
		end = std::chrono::system_clock::now();
		LOG_INF << getName() << ": writing shape attributes back to detail: " << std::chrono::duration<double>(end - start).count() << "s\n";
	}

	unlockInputs();

	return error();
}

bool SOPAssign::updateSharedShapeDataFromParams(OP_Context &context) {
	const fpreal now = context.getTime();

	// -- minimally emitted logging level
	auto ll = static_cast<prt::LogLevel>(evalInt(AssignNodeParams::LOG.getToken(), 0, now));
	mPRTCtx->mLogHandler->setLevel(ll);

	// -- shape classifier attr name
	// TODO: move to param callbacks
	UT_String utNextClsAttrName;
	evalString(utNextClsAttrName, AssignNodeParams::SHAPE_CLS_ATTR.getToken(), 0, now);
	if (utNextClsAttrName.length() > 0)
		mShapeConverter->mShapeClsAttrName.adopt(utNextClsAttrName);
	else
		setString(mShapeConverter->mShapeClsAttrName, CH_STRING_LITERAL, AssignNodeParams::SHAPE_CLS_ATTR.getToken(), 0, now);

	// -- shape classifier attr type
	// TODO: move to param callbacks
	const auto shapeClsAttrTypeChoice = evalInt(AssignNodeParams::SHAPE_CLS_TYPE.getToken(), 0, now);
	switch (shapeClsAttrTypeChoice) {
		case 0: mShapeConverter->mShapeClsType = GA_STORECLASS_STRING; break;
		case 1: mShapeConverter->mShapeClsType = GA_STORECLASS_INT;    break;
		case 2: mShapeConverter->mShapeClsType = GA_STORECLASS_FLOAT;  break;
		default: return false;
	}

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

	return true;
}
