#include "SOPAssign.h"
#include "callbacks.h"
#include "parameter.h"

#include "prt/API.h"

#include "UT/UT_Interrupt.h"


namespace {

const bool DBG = false;

const wchar_t*	ENCODER_ID_CGA_EVALATTR	= L"com.esri.prt.core.AttributeEvalEncoder";

namespace UnitQuad {
const double vertices[] = {0, 0, 0, 0, 0, 1, 1, 0, 1, 1, 0, 0};
const size_t vertexCount = 12;
const uint32_t indices[] = {0, 1, 2, 3};
const size_t indexCount = 4;
const uint32_t faceCounts[] = {4};
const size_t faceCountsCount = 1;
} // namespace UnitQuad

} // namespace anonymous


namespace p4h {

SOPAssign::SOPAssign(const PRTContextUPtr& pCtx, OP_Network* net, const char* name, OP_Operator* op)
: SOP_Node(net, name, op)
, mLogHandler(new log::LogHandler(L"p4h assign", prt::LOG_ERROR))
, mPRTCtx(pCtx)
{
	prt::addLogHandler(mLogHandler.get());
}

SOPAssign::~SOPAssign() {
	prt::removeLogHandler(mLogHandler.get());
}

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

//		start = std::chrono::system_clock::now();
//		updateUserAttributes();
//		end = std::chrono::system_clock::now();
//		LOG_INF << "updateUserAttributes took " << std::chrono::duration<double>(end - start).count() << "s\n";

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
	fpreal now = context.getTime();

	// -- shape classifier attr name
	UT_String utNextClsAttrName;
	evalString(utNextClsAttrName, NODE_PARAM_SHAPE_CLS_ATTR.getToken(), 0, now);
	if (utNextClsAttrName.length() > 0)
		mInitialShapeContext.mShapeClsAttrName.adopt(utNextClsAttrName);
	else
		setString(mInitialShapeContext.mShapeClsAttrName, CH_STRING_LITERAL, NODE_PARAM_SHAPE_CLS_ATTR.getToken(), 0, now);

	// -- shape classifier attr type
	int shapeClsAttrTypeChoice = evalInt(NODE_PARAM_SHAPE_CLS_TYPE.getToken(), 0, now);
	switch (shapeClsAttrTypeChoice) {
		case 0: mInitialShapeContext.mShapeClsType = GA_STORECLASS_STRING; break;
		case 1: mInitialShapeContext.mShapeClsType = GA_STORECLASS_INT;    break;
		case 2: mInitialShapeContext.mShapeClsType = GA_STORECLASS_FLOAT;  break;
		default: return false;
	}

	// -- rule package
	UT_String utNextRPKStr;
	evalString(utNextRPKStr, NODE_PARAM_RPK.getToken(), 0, now);
	boost::filesystem::path nextRPK(utNextRPKStr.toStdString());
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
	UT_String utRuleFile;
	evalString(utRuleFile, NODE_PARAM_RULE_FILE.getToken(), 0, now);
	mInitialShapeContext.mRuleFile = utils::toUTF16FromOSNarrow(utRuleFile.toStdString());
	LOG_DBG << L"got rule file: " << mInitialShapeContext.mRuleFile;

	// -- style
	UT_String utStyle;
	evalString(utStyle, NODE_PARAM_STYLE.getToken(), 0, now);
	mInitialShapeContext.mStyle = utils::toUTF16FromOSNarrow(utStyle.toStdString());

	// -- start rule
	UT_String utStartRule;
	evalString(utStartRule, NODE_PARAM_START_RULE.getToken(), 0, now);
	mInitialShapeContext.mStartRule = utils::toUTF16FromOSNarrow(utStartRule.toStdString());

	// -- random seed
	mInitialShapeContext.mSeed = evalInt(NODE_PARAM_SEED.getToken(), 0, now);

	// -- logger
	auto ll = static_cast<prt::LogLevel>(evalInt(NODE_PARAM_LOG.getToken(), 0, now));
	mLogHandler->setLevel(ll);
	mLogHandler->setName(utils::toUTF16FromOSNarrow(getName().toStdString()));

	LOG_DBG << "handleParams done.";
	return true;
}

//namespace {

void getDefaultRuleAttributeValues(
		AttributeMapBuilderPtr& amb,
		CacheObjectPtr& cache,
		const ResolveMapUPtr& resolveMap,
		const std::wstring& cgbKey,
		const std::wstring& startRule
) {
    // 1. resolve CGB
    // 2. get rule info
    // 3. get hidden annotation


	HoudiniGeometry hg(nullptr, amb.get());
	AttributeMapPtr emptyAttrMap(amb->createAttributeMapAndReset());

	InitialShapeBuilderPtr isb(prt::InitialShapeBuilder::create());
	isb->setGeometry(
			UnitQuad::vertices, UnitQuad::vertexCount, UnitQuad::indices, UnitQuad::indexCount, UnitQuad::faceCounts,
			UnitQuad::faceCountsCount
	);
	isb->setAttributes(cgbKey.c_str(), startRule.c_str(), 666, L"temp", emptyAttrMap.get(), resolveMap.get());

	prt::Status status = prt::STATUS_UNSPECIFIED_ERROR;
	InitialShapePtr is(isb->createInitialShapeAndReset(&status));
	const prt::InitialShape* iss[1] = {is.get()};

	EncoderInfoPtr encInfo(prt::createEncoderInfo(ENCODER_ID_CGA_EVALATTR));
	const prt::AttributeMap* encOpts = nullptr;
	encInfo->createValidatedOptionsAndStates(nullptr, &encOpts);

	const wchar_t* encs[1] = {ENCODER_ID_CGA_EVALATTR};
	const prt::AttributeMap* encsOpts[1] = {encOpts};

	prt::Status stat = prt::generate(iss, 1, nullptr, encs, 1, encsOpts, &hg, cache.get(), nullptr, nullptr, nullptr);
	if (stat != prt::STATUS_OK) {
		LOG_ERR << "prt::generate() failed with status: '" << prt::getStatusDescription(stat) << "' (" << stat << ")";
	}

	encOpts->destroy();
}

//} // namespace

bool SOPAssign::updateRulePackage(const boost::filesystem::path& nextRPK, fpreal time) {
	if (nextRPK == mInitialShapeContext.mRPK)
		return true;
	if (!boost::filesystem::exists(nextRPK))
		return false;

	std::wstring nextRPKURI = utils::toFileURI(nextRPK);
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
	std::wstring cgbKey = cgbs.front().first;
	std::wstring cgbURI = cgbs.front().second;
	LOG_DBG << "cgbKey = " << cgbKey;
	LOG_DBG << "cgbURI = " << cgbURI;

	prt::Status status = prt::STATUS_UNSPECIFIED_ERROR;
	RuleFileInfoPtr info(prt::createRuleFileInfo(cgbURI.c_str(), mPRTCtx->mPRTCache.get(), &status));
	if (!info || status != prt::STATUS_OK) {
		LOG_ERR << "failed to get rule file info";
		return false;
	}

	// get first rule (start rule if possible)
	if (info->getNumRules() == 0) {
		LOG_ERR << "rule file does not contain any rules";
		return false;
	}
	auto startRuleIdx = size_t(-1);
	for (size_t ri = 0; ri < info->getNumRules(); ri++) {
		const prt::RuleFileInfo::Entry* re = info->getRule(ri);
		for (size_t ai = 0; ai < re->getNumAnnotations(); ai++) {
			if (std::wcscmp(re->getAnnotation(ai)->getName(), L"@StartRule") == 0) {
				startRuleIdx = ri;
				break;
			}
		}
	}
	if (startRuleIdx == size_t(-1))
		startRuleIdx = 0;
	const prt::RuleFileInfo::Entry* re = info->getRule(startRuleIdx);
	std::wstring fqStartRule = re->getName();
	std::vector<std::wstring> startRuleComponents;
	boost::split(startRuleComponents, fqStartRule, boost::is_any_of(L"$"));
	LOG_DBG << "first start rule: " << fqStartRule;

	// update node
	mInitialShapeContext.mRPK = nextRPK;
	mPRTCtx->mPRTCache->flushAll();
	{
		mInitialShapeContext.mRuleFile = cgbKey;
		UT_String val(utils::toOSNarrowFromUTF16(mInitialShapeContext.mRuleFile));
		setString(val, CH_STRING_LITERAL, NODE_PARAM_RULE_FILE.getToken(), 0, time);
	}
	{
		mInitialShapeContext.mStyle = startRuleComponents[0];
		UT_String val(utils::toOSNarrowFromUTF16(mInitialShapeContext.mStyle));
		setString(val, CH_STRING_LITERAL, NODE_PARAM_STYLE.getToken(), 0, time);
	}
	{
		mInitialShapeContext.mStartRule = startRuleComponents[1];
		UT_String val(utils::toOSNarrowFromUTF16(mInitialShapeContext.mStartRule));
		setString(val, CH_STRING_LITERAL, NODE_PARAM_START_RULE.getToken(), 0, time);
	}
	{
		mInitialShapeContext.mSeed = 0;
		setInt(NODE_PARAM_SEED.getToken(), 0, time, mInitialShapeContext.mSeed);
	}
	LOG_DBG << "updateRulePackage done: mRuleFile = " << mInitialShapeContext.mRuleFile << ", mStyle = " <<
	mInitialShapeContext.mStyle << ", mStartRule = " << mInitialShapeContext.mStartRule;

	// eval rule attribute values
	AttributeMapBuilderPtr amb(prt::AttributeMapBuilder::create());
	getDefaultRuleAttributeValues(amb, mPRTCtx->mPRTCache, resolveMap, mInitialShapeContext.mRuleFile, fqStartRule);
	mInitialShapeContext.mRuleAttributeValues.reset(amb->createAttributeMap());

	LOG_DBG << "mRuleAttributeValues = " << utils::objectToXML(mInitialShapeContext.mRuleAttributeValues.get());

	return true;
}

} // namespace p4h
