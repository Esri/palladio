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
constexpr const wchar_t* CGA_ANNOTATION_START_RULE = L"@StartRule";
constexpr const size_t   CGA_NO_START_RULE_FOUND   = size_t(-1);

void evaluateDefaultRuleAttributes(
		ShapeData& shapeData,
		const ShapeConverterUPtr& sharedShapeData,
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
	const ResolveMapUPtr& resolveMap = prtCtx->getResolveMap(sharedShapeData->mRPK);

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
				sharedShapeData->mRuleFile.c_str(),
				sharedShapeData->mStartRule.c_str(),
				sharedShapeData->mSeed,
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
	AttrEvalCallbacks aec(shapeData.mRuleAttributeBuilders, sharedShapeData->mRuleFileInfo);
	const prt::Status stat = prt::generate(iss.data(), iss.size(), nullptr, encs, encsCount, encsOpts, &aec, prtCtx->mPRTCache.get(), nullptr, nullptr, nullptr);
	if (stat != prt::STATUS_OK) {
		LOG_ERR << "assign: prt::generate() failed with status: '" << prt::getStatusDescription(stat) << "' (" << stat << ")";
	}

	if (encOpts)
		encOpts->destroy();
}

} // namespace


SOPAssign::SOPAssign(const PRTContextUPtr& pCtx, OP_Network* net, const char* name, OP_Operator* op)
: SOP_Node(net, name, op), mPRTCtx(pCtx), mSharedShapeData(new ShapeConverter()) { }

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
		mSharedShapeData->get(gdp, shapeData, mPRTCtx);
		end = std::chrono::system_clock::now();
		LOG_INF << getName() << ": extracting shapes from detail: " << std::chrono::duration<double>(end - start).count() << "s\n";

		start = std::chrono::system_clock::now();
		evaluateDefaultRuleAttributes(shapeData, mSharedShapeData, mPRTCtx);
		end = std::chrono::system_clock::now();
		LOG_INF << getName() << ": evaluating default rule attributes on shapes: " << std::chrono::duration<double>(end - start).count() << "s\n";

		start = std::chrono::system_clock::now();
		mSharedShapeData->put(gdp, shapeData);
		end = std::chrono::system_clock::now();
		LOG_INF << getName() << ": writing shape attributes back to detail: " << std::chrono::duration<double>(end - start).count() << "s\n";
	}

	unlockInputs();

	return error();
}

bool SOPAssign::updateSharedShapeDataFromParams(OP_Context &context) {
	const fpreal now = context.getTime();

	// -- logger
	auto ll = static_cast<prt::LogLevel>(evalInt(AssignNodeParams::LOG.getToken(), 0, now));
	mPRTCtx->mLogHandler->setLevel(ll);

	// -- shape classifier attr name
	UT_String utNextClsAttrName;
	evalString(utNextClsAttrName, AssignNodeParams::SHAPE_CLS_ATTR.getToken(), 0, now);
	if (utNextClsAttrName.length() > 0)
		mSharedShapeData->mShapeClsAttrName.adopt(utNextClsAttrName);
	else
		setString(mSharedShapeData->mShapeClsAttrName, CH_STRING_LITERAL, AssignNodeParams::SHAPE_CLS_ATTR.getToken(), 0, now);

	// -- shape classifier attr type
	const auto shapeClsAttrTypeChoice = evalInt(AssignNodeParams::SHAPE_CLS_TYPE.getToken(), 0, now);
	switch (shapeClsAttrTypeChoice) {
		case 0: mSharedShapeData->mShapeClsType = GA_STORECLASS_STRING; break;
		case 1: mSharedShapeData->mShapeClsType = GA_STORECLASS_INT;    break;
		case 2: mSharedShapeData->mShapeClsType = GA_STORECLASS_FLOAT;  break;
		default: return false;
	}

	// -- rule package
	const auto nextRPK = [this,now](){
		UT_String utNextRPKStr;
		evalString(utNextRPKStr, AssignNodeParams::RPK.getToken(), 0, now);
		return boost::filesystem::path(utNextRPKStr.toStdString());
	}();
	if (!updateRulePackage(nextRPK, now)) {
		const PRM_Parm& p = getParm(AssignNodeParams::RPK.getToken());
		UT_String expr;
		p.getExpressionOnly(now, expr, 0, 0);
		if (expr.length() == 0) { // if not an expression ...
			UT_String val(mSharedShapeData->mRPK.string());
			setString(val, CH_STRING_LITERAL, AssignNodeParams::RPK.getToken(), 0, now); // ... reset to current value
		}
		return false;
	}

	// -- rule file
	mSharedShapeData->mRuleFile = [this,now](){
		UT_String utRuleFile;
		evalString(utRuleFile, AssignNodeParams::RULE_FILE.getToken(), 0, now);
		return toUTF16FromOSNarrow(utRuleFile.toStdString());
	}();

	// -- rule style
	mSharedShapeData->mStyle = [this,now](){
		UT_String utStyle;
		evalString(utStyle, AssignNodeParams::STYLE.getToken(), 0, now);
		return toUTF16FromOSNarrow(utStyle.toStdString());
	}();

	// -- start rule
	mSharedShapeData->mStartRule = [this,now](){
		UT_String utStartRule;
		evalString(utStartRule, AssignNodeParams::START_RULE.getToken(), 0, now);
		return toUTF16FromOSNarrow(utStartRule.toStdString());
	}();

	// -- random seed
	mSharedShapeData->mSeed = evalInt(AssignNodeParams::SEED.getToken(), 0, now);

	return true;
}

bool SOPAssign::updateRulePackage(const boost::filesystem::path& nextRPK, fpreal time) {
	if (nextRPK == mSharedShapeData->mRPK)
		return true;
	if (!boost::filesystem::exists(nextRPK))
		return false;

	LOG_DBG << L"nextRPK = " << nextRPK;

	// rebuild assets map
	const ResolveMapUPtr& resolveMap = mPRTCtx->getResolveMap(nextRPK);
	if (!resolveMap ) {
		LOG_ERR << "failed to create resolve map from '" << nextRPK << "', aborting.";
		return false;
	}

	// get first rule file
	std::vector<std::pair<std::wstring, std::wstring>> cgbs; // key -> uri
	getCGBs(resolveMap, cgbs);
	if (cgbs.empty()) {
		LOG_ERR << "no rule files found in rule package";
		return false;
	}
	const std::wstring cgbKey = cgbs.front().first;
	const std::wstring cgbURI = cgbs.front().second;
	LOG_DBG << "cgbKey = " << cgbKey << ", " << "cgbURI = " << cgbURI;

	prt::Status status = prt::STATUS_UNSPECIFIED_ERROR;
	mSharedShapeData->mRuleFileInfo.reset(prt::createRuleFileInfo(cgbURI.c_str(), mPRTCtx->mPRTCache.get(), &status));
	if (!mSharedShapeData->mRuleFileInfo || (status != prt::STATUS_OK) || (mSharedShapeData->mRuleFileInfo->getNumRules() == 0)) {
		LOG_ERR << "failed to get rule file info or rule file does not contain any rules";
		return false;
	}

	// find start rule (first annotated start rule or just first rule as fallback)
	auto findStartRule = [](const RuleFileInfoUPtr& info) -> std::wstring {
		auto startRuleIdx = CGA_NO_START_RULE_FOUND;
		for (size_t ri = 0; ri < info->getNumRules(); ri++) {
			const prt::RuleFileInfo::Entry *re = info->getRule(ri);
			for (size_t ai = 0; ai < re->getNumAnnotations(); ai++) {
				if (std::wcscmp(re->getAnnotation(ai)->getName(), CGA_ANNOTATION_START_RULE) == 0) {
					startRuleIdx = ri;
					break;
				}
			}
		}
		if (startRuleIdx == CGA_NO_START_RULE_FOUND)
			startRuleIdx = 0;
		const prt::RuleFileInfo::Entry *re = info->getRule(startRuleIdx);
		return { re->getName() };
	};
	const std::wstring fqStartRule = findStartRule(mSharedShapeData->mRuleFileInfo);

	// get style/name from start rule
	auto getStartRuleComponents = [](const std::wstring& fqRule) -> std::pair<std::wstring,std::wstring> {
		std::vector<std::wstring> startRuleComponents;
		boost::split(startRuleComponents, fqRule, boost::is_any_of(L"$")); // TODO: split is overkill
		return { startRuleComponents[0], startRuleComponents[1]};
	};
	const auto startRuleComponents = getStartRuleComponents(fqStartRule);
	LOG_DBG << "start rule: style = " << startRuleComponents.first << ", name = " << startRuleComponents.second;

	// update node
	mSharedShapeData->mRPK = nextRPK;
	mPRTCtx->mPRTCache->flushAll();
	{
		mSharedShapeData->mRuleFile = cgbKey;
		UT_String val(toOSNarrowFromUTF16(mSharedShapeData->mRuleFile));
		setString(val, CH_STRING_LITERAL, AssignNodeParams::RULE_FILE.getToken(), 0, time);
	}
	{
		mSharedShapeData->mStyle = startRuleComponents.first;
		UT_String val(toOSNarrowFromUTF16(mSharedShapeData->mStyle));
		setString(val, CH_STRING_LITERAL, AssignNodeParams::STYLE.getToken(), 0, time);
	}
	{
		mSharedShapeData->mStartRule = startRuleComponents.second;
		UT_String val(toOSNarrowFromUTF16(mSharedShapeData->mStartRule));
		setString(val, CH_STRING_LITERAL, AssignNodeParams::START_RULE.getToken(), 0, time);
	}
	{
		mSharedShapeData->mSeed = 0;
		setInt(AssignNodeParams::SEED.getToken(), 0, time, mSharedShapeData->mSeed);
	}
	LOG_DBG << "updateRulePackage done: mRuleFile = " << mSharedShapeData->mRuleFile << ", mStyle = " << mSharedShapeData->mStyle << ", mStartRule = " << mSharedShapeData->mStartRule;

	return true;
}
