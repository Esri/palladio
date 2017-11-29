#include "SOPAssign.h"
#include "callbacks.h"
#include "parameter.h"

#include "prt/API.h"

#include "UT/UT_Interrupt.h"


namespace {

constexpr bool           DBG                       = false;
constexpr const wchar_t* ENCODER_ID_CGA_EVALATTR   = L"com.esri.prt.core.AttributeEvalEncoder";
constexpr const wchar_t* CGA_ANNOTATION_START_RULE = L"@StartRule";
constexpr const size_t   CGA_NO_START_RULE_FOUND   = size_t(-1);

namespace UnitQuad {
	constexpr double   VERTICES[]        = { 0, 0, 0, 0, 0, 1, 1, 0, 1, 1, 0, 0 };
	constexpr size_t   VERTICES_COUNT    = sizeof(VERTICES)/sizeof(VERTICES[0]);
	constexpr uint32_t INDICES[]         = { 0, 1, 2, 3 };
	constexpr size_t   INDICES_COUNT     = sizeof(INDICES)/sizeof(INDICES[0]);
	constexpr uint32_t FACE_COUNTS[]     = { 4 };
	constexpr size_t   FACE_COUNTS_COUNT = sizeof(FACE_COUNTS)/sizeof(FACE_COUNTS[0]);
} // namespace UnitQuad

} // namespace


namespace p4h {

SOPAssign::SOPAssign(const PRTContextUPtr& pCtx, OP_Network* net, const char* name, OP_Operator* op)
: SOP_Node(net, name, op), mPRTCtx(pCtx) { }

OP_ERROR SOPAssign::cookMySop(OP_Context& context) {
	LOG_DBG << "SOPAssign::cookMySop";

	if (!handleParams(context))
		return UT_ERROR_ABORT;

	if (lockInputs(context) >= UT_ERROR_ABORT) {
		LOG_DBG << "lockInputs error";
		return error();
	}
	duplicateSource(0, context);

	if (error() < UT_ERROR_ABORT && cookInputGroups(context) < UT_ERROR_ABORT) {
		UT_AutoInterrupt progress("Assigning CityEngine attributes...");

		std::chrono::time_point<std::chrono::system_clock> start, end;

		start = std::chrono::system_clock::now();
		mInitialShapeContext.put(gdp);
		end = std::chrono::system_clock::now();
		LOG_INF << "writing isc into gdp took " << std::chrono::duration<double>(end - start).count() << "s\n";
	}

	unlockInputs();

	LOG_DBG << "SOPAssign::cookMySop done";
	return error();
}

bool SOPAssign::handleParams(OP_Context& context) {
	LOG_DBG << "handleParams begin";
	const fpreal now = context.getTime();

	// -- logger
	auto ll = static_cast<prt::LogLevel>(evalInt(NODE_PARAM_LOG.getToken(), 0, now));
	mPRTCtx->mLogHandler->setLevel(ll);

	// -- shape classifier attr name
	UT_String utNextClsAttrName;
	evalString(utNextClsAttrName, NODE_PARAM_SHAPE_CLS_ATTR.getToken(), 0, now);
	if (utNextClsAttrName.length() > 0)
		mInitialShapeContext.mShapeClsAttrName.adopt(utNextClsAttrName);
	else
		setString(mInitialShapeContext.mShapeClsAttrName, CH_STRING_LITERAL, NODE_PARAM_SHAPE_CLS_ATTR.getToken(), 0, now);

	// -- shape classifier attr type
	const int shapeClsAttrTypeChoice = evalInt(NODE_PARAM_SHAPE_CLS_TYPE.getToken(), 0, now);
	switch (shapeClsAttrTypeChoice) {
		case 0: mInitialShapeContext.mShapeClsType = GA_STORECLASS_STRING; break;
		case 1: mInitialShapeContext.mShapeClsType = GA_STORECLASS_INT;    break;
		case 2: mInitialShapeContext.mShapeClsType = GA_STORECLASS_FLOAT;  break;
		default: return false;
	}

	// -- rule package
	const auto nextRPK = [this,now](){
		UT_String utNextRPKStr;
		evalString(utNextRPKStr, NODE_PARAM_RPK.getToken(), 0, now);
		return boost::filesystem::path(utNextRPKStr.toStdString());
	}();
	if (!updateRulePackage(nextRPK, now)) {
		const PRM_Parm& p = getParm(NODE_PARAM_RPK.getToken());
		UT_String expr;
		p.getExpressionOnly(now, expr, 0, 0);
		if (expr.length() == 0) { // if not an expression ...
			UT_String val(mInitialShapeContext.mRPK.string());
			setString(val, CH_STRING_LITERAL, NODE_PARAM_RPK.getToken(), 0, now); // ... reset to current value
		}
		return false;
	}

	// -- rule file
	mInitialShapeContext.mRuleFile = [this,now](){
		UT_String utRuleFile;
		evalString(utRuleFile, NODE_PARAM_RULE_FILE.getToken(), 0, now);
		return utils::toUTF16FromOSNarrow(utRuleFile.toStdString());
	}();
	LOG_DBG << L"got rule file: " << mInitialShapeContext.mRuleFile;

	// -- style
	mInitialShapeContext.mStyle = [this,now](){
		UT_String utStyle;
		evalString(utStyle, NODE_PARAM_STYLE.getToken(), 0, now);
		return utils::toUTF16FromOSNarrow(utStyle.toStdString());
	}();

	// -- start rule
	mInitialShapeContext.mStartRule = [this,now](){
		UT_String utStartRule;
		evalString(utStartRule, NODE_PARAM_START_RULE.getToken(), 0, now);
		return utils::toUTF16FromOSNarrow(utStartRule.toStdString());
	}();

	// -- random seed
	mInitialShapeContext.mSeed = evalInt(NODE_PARAM_SEED.getToken(), 0, now);

	LOG_DBG << "handleParams done.";
	return true;
}

bool SOPAssign::updateRulePackage(const boost::filesystem::path& nextRPK, fpreal time) {
	if (nextRPK == mInitialShapeContext.mRPK)
		return true;
	if (!boost::filesystem::exists(nextRPK))
		return false;

	const std::wstring nextRPKURI = utils::toFileURI(nextRPK);
	LOG_DBG << L"nextRPKURI = " << nextRPKURI;

	// rebuild assets map
	const ResolveMapUPtr& resolveMap = mPRTCtx->getResolveMap(nextRPKURI);
	if (!resolveMap ) {
		LOG_ERR << "failed to create resolve map from '" << nextRPKURI << "', aborting.";
		return false;
	}

	// get first rule file
	std::vector<std::pair<std::wstring, std::wstring>> cgbs; // key -> uri
	utils::getCGBs(resolveMap, cgbs);
	if (cgbs.empty()) {
		LOG_ERR << "no rule files found in rule package";
		return false;
	}
	const std::wstring cgbKey = cgbs.front().first;
	const std::wstring cgbURI = cgbs.front().second;
	LOG_DBG << "cgbKey = " << cgbKey << ", " << "cgbURI = " << cgbURI;

	prt::Status status = prt::STATUS_UNSPECIFIED_ERROR;
	const RuleFileInfoPtr ruleFileInfo(prt::createRuleFileInfo(cgbURI.c_str(), mPRTCtx->mPRTCache.get(), &status));
	if (!ruleFileInfo || (status != prt::STATUS_OK) || (ruleFileInfo->getNumRules() == 0)) {
		LOG_ERR << "failed to get rule file info or rule file does not contain any rules";
		return false;
	}

	// find start rule (first annotated start rule or just first rule as fallback)
	auto findStartRule = [](const RuleFileInfoPtr& info) -> std::wstring {
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
	const std::wstring fqStartRule = findStartRule(ruleFileInfo);

	// get style/name from start rule
	auto getStartRuleComponents = [](const std::wstring& fqRule) -> std::pair<std::wstring,std::wstring> {
		std::vector<std::wstring> startRuleComponents;
		boost::split(startRuleComponents, fqRule, boost::is_any_of(L"$")); // TODO: split is overkill
		return { startRuleComponents[0], startRuleComponents[1]};
	};
	const auto startRuleComponents = getStartRuleComponents(fqStartRule);
	LOG_DBG << "start rule: style = " << startRuleComponents.first << ", name = " << startRuleComponents.second;

	// update node
	mInitialShapeContext.mRPK = nextRPK;
	mPRTCtx->mPRTCache->flushAll();
	{
		mInitialShapeContext.mRuleFile = cgbKey;
		UT_String val(utils::toOSNarrowFromUTF16(mInitialShapeContext.mRuleFile));
		setString(val, CH_STRING_LITERAL, NODE_PARAM_RULE_FILE.getToken(), 0, time);
	}
	{
		mInitialShapeContext.mStyle = startRuleComponents.first;
		UT_String val(utils::toOSNarrowFromUTF16(mInitialShapeContext.mStyle));
		setString(val, CH_STRING_LITERAL, NODE_PARAM_STYLE.getToken(), 0, time);
	}
	{
		mInitialShapeContext.mStartRule = startRuleComponents.second;
		UT_String val(utils::toOSNarrowFromUTF16(mInitialShapeContext.mStartRule));
		setString(val, CH_STRING_LITERAL, NODE_PARAM_START_RULE.getToken(), 0, time);
	}
	{
		mInitialShapeContext.mSeed = 0;
		setInt(NODE_PARAM_SEED.getToken(), 0, time, mInitialShapeContext.mSeed);
	}
	LOG_DBG << "updateRulePackage done: mRuleFile = " << mInitialShapeContext.mRuleFile << ", mStyle = " << mInitialShapeContext.mStyle << ", mStartRule = " << mInitialShapeContext.mStartRule;

	// evaluate default rule attribute values
	AttributeMapBuilderPtr amb(prt::AttributeMapBuilder::create());
	getDefaultRuleAttributeValues(amb, mPRTCtx->mPRTCache, resolveMap, mInitialShapeContext.mRuleFile, fqStartRule, ruleFileInfo);
	mInitialShapeContext.mRuleAttributeValues.reset(amb->createAttributeMap());

	LOG_DBG << "mRuleAttributeValues = " << utils::objectToXML(mInitialShapeContext.mRuleAttributeValues.get());

	return true;
}

void getDefaultRuleAttributeValues(
		AttributeMapBuilderPtr& amb,
		CacheObjectPtr& cache,
		const ResolveMapUPtr& resolveMap,
		const std::wstring& cgbKey,
		const std::wstring& fqStartRule,
		const RuleFileInfoPtr& ruleFileInfo
) {
	AttrEvalCallbacks aec(amb, ruleFileInfo);
	AttributeMapPtr emptyAttrMap(amb->createAttributeMapAndReset());

	InitialShapeBuilderPtr isb(prt::InitialShapeBuilder::create());
	isb->setGeometry(
			UnitQuad::VERTICES, UnitQuad::VERTICES_COUNT,
			UnitQuad::INDICES, UnitQuad::INDICES_COUNT,
			UnitQuad::FACE_COUNTS, UnitQuad::FACE_COUNTS_COUNT);
	isb->setAttributes(cgbKey.c_str(), fqStartRule.c_str(), 666, L"temp", emptyAttrMap.get(), resolveMap.get());

	prt::Status status = prt::STATUS_UNSPECIFIED_ERROR;
	const InitialShapePtr is(isb->createInitialShapeAndReset(&status));
	const prt::InitialShape* iss[] = { is.get() };
	const size_t issCounts = sizeof(iss) / sizeof(iss[0]);

	const EncoderInfoPtr encInfo(prt::createEncoderInfo(ENCODER_ID_CGA_EVALATTR));
	const prt::AttributeMap* encOpts = nullptr;
	encInfo->createValidatedOptionsAndStates(nullptr, &encOpts);

	constexpr const wchar_t* encs[] = { ENCODER_ID_CGA_EVALATTR };
	const prt::AttributeMap* encsOpts[] = { encOpts };

	const prt::Status stat = prt::generate(iss, issCounts, nullptr, encs, 1, encsOpts, &aec, cache.get(), nullptr, nullptr, nullptr);
	if (stat != prt::STATUS_OK) {
		LOG_ERR << "prt::generate() failed with status: '" << prt::getStatusDescription(stat) << "' (" << stat << ")";
	}

	encOpts->destroy();
}

} // namespace p4h
