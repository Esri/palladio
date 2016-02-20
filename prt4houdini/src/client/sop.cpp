#include "client/sop.h"
#include "client/shapegen.h"
#include "client/callbacks.h"
#include "client/utils.h"

#include "prt/API.h"
#include "prt/FlexLicParams.h"

#ifndef WIN32
#	pragma GCC diagnostic push
#	pragma GCC diagnostic ignored "-Wunused-local-typedefs"
#	pragma GCC diagnostic ignored "-Wattributes"
#endif
#include "GU/GU_Detail.h"
#include "GU/GU_PrimPoly.h"
#include "PRM/PRM_Include.h"
#include "PRM/PRM_SpareData.h"
#include "PRM/PRM_ChoiceList.h"
#include "PRM/PRM_Parm.h"
#include "OP/OP_Operator.h"
#include "OP/OP_OperatorTable.h"
#include "OP/OP_Director.h"
#include "SOP/SOP_Guide.h"
#include "GOP/GOP_GroupParse.h"
#include "GEO/GEO_PolyCounts.h"
#include "PI/PI_EditScriptedParms.h"
#include "UT/UT_DSOVersion.h"
#include "UT/UT_Matrix3.h"
#include "UT/UT_Matrix4.h"
#include "UT/UT_Exit.h"
#include "UT/UT_Interrupt.h"
#include "SYS/SYS_Math.h"
#include <HOM/HOM_Module.h>
#include <HOM/HOM_SopNode.h>

#include "boost/foreach.hpp"
#include "boost/filesystem.hpp"
#include "boost/algorithm/string.hpp"

#ifdef WIN32
#	include "boost/assign.hpp"
#endif

#ifndef WIN32
#	pragma GCC diagnostic pop
#endif

#include <vector>
#include <cwchar>


namespace {

const bool DBG = false;

// global prt settings
const prt::LogLevel	PRT_LOG_LEVEL		= prt::LOG_DEBUG;
const char*			PRT_LIB_SUBDIR		= "prtlib";
const char*			FILE_FLEXNET_LIB	= "flexnet_prt";
const wchar_t*		FILE_CGA_ERROR		= L"CGAErrors.txt";
const wchar_t*		FILE_CGA_PRINT		= L"CGAPrint.txt";

// some encoder IDs
const wchar_t*	ENCODER_ID_CGA_EVALATTR	= L"com.esri.prt.core.AttributeEvalEncoder";
const wchar_t*	ENCODER_ID_CGA_ERROR	= L"com.esri.prt.core.CGAErrorEncoder";
const wchar_t*	ENCODER_ID_CGA_PRINT	= L"com.esri.prt.core.CGAPrintEncoder";
const wchar_t*	ENCODER_ID_HOUDINI		= L"HoudiniEncoder";

// objects with same lifetime as PRT process
const prt::Object* prtLicHandle = 0;

void dsoExit(void*) {
	if (prtLicHandle)
		prtLicHandle->destroy(); // prt shutdown
}

const PRM_Name NODE_PARAM_SHAPE_CLS_ATTR("shapeClsAttr",	"Shape Classifier");
const PRM_Name NODE_PARAM_SHAPE_CLS_TYPE("shapeClsType",	"Shape Classifier Type");
const PRM_Name NODE_PARAM_RPK			("rpk",				"Rule Package");
const PRM_Name NODE_PARAM_RULE_FILE		("ruleFile", 		"Rule File");
const PRM_Name NODE_PARAM_STYLE			("style",			"Style");
const PRM_Name NODE_PARAM_START_RULE	("startRule",		"Start Rule");
const PRM_Name NODE_PARAM_SEED			("seed",			"Random Seed");
const PRM_Name NODE_PARAM_LOG			("logLevel",		"Log Level");

PRM_Name NODE_PARAM_NAMES[] = {
	NODE_PARAM_SHAPE_CLS_ATTR,
	NODE_PARAM_SHAPE_CLS_TYPE,
	NODE_PARAM_RPK,
	NODE_PARAM_RULE_FILE,
	NODE_PARAM_STYLE,
	NODE_PARAM_START_RULE,
	NODE_PARAM_SEED,
	NODE_PARAM_LOG
};

PRM_Default rpkDefault(0, "$HIP/$F.rpk");
PRM_Default startRuleDefault(0, "Start");
PRM_Default ruleFileDefault(0, "<none>");

PRM_ChoiceList ruleFileMenu(static_cast<PRM_ChoiceListType>(PRM_CHOICELIST_EXCLUSIVE | PRM_CHOICELIST_REPLACE), &p4h::SOP_PRT::buildRuleFileMenu);
PRM_ChoiceList startRuleMenu(static_cast<PRM_ChoiceListType>(PRM_CHOICELIST_EXCLUSIVE | PRM_CHOICELIST_REPLACE), &p4h::SOP_PRT::buildStartRuleMenu);

PRM_Name shapeClsTypes[] = {
		PRM_Name("STRING", "String"),
		PRM_Name("INT", "Integer"),
		PRM_Name("FLOAT", "Float"),
		PRM_Name(0)
};
PRM_ChoiceList shapeClsTypeMenu((PRM_ChoiceListType)(PRM_CHOICELIST_EXCLUSIVE | PRM_CHOICELIST_REPLACE), shapeClsTypes);
PRM_Default shapeClsTypeDefault(0, "INT");

PRM_Name logNames[] = {
		PRM_Name("TRACE", "trace"), // TODO: eventually, remove this and offset index by 1
		PRM_Name("DEBUG", "debug"),
		PRM_Name("INFO", "info"),
		PRM_Name("WARNING", "warning"),
		PRM_Name("ERROR", "error"),
		PRM_Name("FATAL", "fatal"),
		PRM_Name(0)
};
PRM_ChoiceList logMenu((PRM_ChoiceListType)(PRM_CHOICELIST_EXCLUSIVE | PRM_CHOICELIST_REPLACE), logNames);
PRM_Default logDefault(0, "ERROR");

PRM_Template NODE_PARAM_TEMPLATES[] = {
		PRM_Template(PRM_STRING,	1, &NODE_PARAM_NAMES[0],	PRMoneDefaults),
		PRM_Template((PRM_Type)PRM_ORD, PRM_Template::PRM_EXPORT_MAX, 1, &NODE_PARAM_NAMES[1], &shapeClsTypeDefault, &shapeClsTypeMenu),
		PRM_Template(PRM_FILE,		1, &NODE_PARAM_NAMES[2],	&rpkDefault, 0, 0, 0, &PRM_SpareData::fileChooserModeRead),
		PRM_Template(PRM_STRING,	1, &NODE_PARAM_NAMES[3],	PRMoneDefaults, &ruleFileMenu),
		PRM_Template(PRM_STRING,	1, &NODE_PARAM_NAMES[4],	PRMoneDefaults),
		PRM_Template(PRM_STRING,	1, &NODE_PARAM_NAMES[5],	PRMoneDefaults, &startRuleMenu),
		PRM_Template(PRM_INT,		1, &NODE_PARAM_NAMES[6],	PRMoneDefaults),
		PRM_Template((PRM_Type)PRM_ORD, PRM_Template::PRM_EXPORT_MAX, 1, &NODE_PARAM_NAMES[7], &logDefault, &logMenu),
		PRM_Template()
};

} // namespace anonymous


// TODO: add support for multiple nodes
void newSopOperator(OP_OperatorTable *table) {
	UT_Exit::addExitCallback(dsoExit);

	boost::filesystem::path sopPath;
	p4h::utils::getPathToCurrentModule(sopPath);

	prt::FlexLicParams flp;
	std::string libflexnet = p4h::utils::getSharedLibraryPrefix() + FILE_FLEXNET_LIB + p4h::utils::getSharedLibrarySuffix();
	std::string libflexnetPath = (sopPath.parent_path() / libflexnet).string();
	flp.mActLibPath = libflexnetPath.c_str();
	flp.mFeature = "CityEngAdvFx";
	flp.mHostName = "";

	std::wstring libPath = (sopPath.parent_path() / PRT_LIB_SUBDIR).wstring();
	const wchar_t* extPaths[] = { libPath.c_str() };

	prt::Status status = prt::STATUS_UNSPECIFIED_ERROR;
	prtLicHandle = prt::init(extPaths, 1, PRT_LOG_LEVEL, &flp, &status); // TODO: add UI for log level control

	if (prtLicHandle == 0 || status != prt::STATUS_OK)
		return;

	const size_t minSources = 1;
	const size_t maxSources = 1;
	table->addOperator(new OP_Operator("prt4houdini", "prt4houdini", p4h::SOP_PRT::create,
			NODE_PARAM_TEMPLATES, minSources, maxSources, 0)
	);
}


namespace p4h {

OP_Node* SOP_PRT::create(OP_Network *net, const char *name, OP_Operator *op) {
	return new SOP_PRT(net, name, op);
}

SOP_PRT::SOP_PRT(OP_Network *net, const char *name, OP_Operator *op)
: SOP_Node(net, name, op)
, mPRTCache(prt::CacheObject::create(prt::CacheObject::CACHE_TYPE_DEFAULT))
, mLogHandler(new log::LogHandler(utils::toUTF16FromOSNarrow(name), prt::LOG_ERROR))
{
	prt::addLogHandler(mLogHandler.get());

	AttributeMapBuilderPtr optionsBuilder(prt::AttributeMapBuilder::create());

	const prt::AttributeMap* encoderOptions = optionsBuilder->createAttributeMapAndReset();
	mHoudiniEncoderOptions = utils::createValidatedOptions(ENCODER_ID_HOUDINI, encoderOptions);
	encoderOptions->destroy();

	optionsBuilder->setString(L"name", FILE_CGA_ERROR);
	const prt::AttributeMap* errOptions = optionsBuilder->createAttributeMapAndReset();
	mCGAErrorOptions = utils::createValidatedOptions(ENCODER_ID_CGA_ERROR, errOptions);
	errOptions->destroy();

	optionsBuilder->setString(L"name", FILE_CGA_PRINT);
	const prt::AttributeMap* printOptions = optionsBuilder->createAttributeMapAndReset();
	mCGAPrintOptions = utils::createValidatedOptions(ENCODER_ID_CGA_PRINT, printOptions);
	printOptions->destroy();

#ifdef WIN32
	mAllEncoders = boost::assign::list_of(ENCODER_ID_HOUDINI)(ENCODER_ID_CGA_ERROR)(ENCODER_ID_CGA_PRINT);
	mAllEncoderOptions = boost::assign::list_of(mHoudiniEncoderOptions)(mCGAErrorOptions)(mCGAPrintOptions);
#else
	mAllEncoders = { ENCODER_ID_HOUDINI, ENCODER_ID_CGA_ERROR, ENCODER_ID_CGA_PRINT };
	mAllEncoderOptions = { mHoudiniEncoderOptions, mCGAErrorOptions, mCGAPrintOptions };
#endif
}

SOP_PRT::~SOP_PRT() {
	mHoudiniEncoderOptions->destroy();
	mCGAErrorOptions->destroy();
	mCGAPrintOptions->destroy();

	mPRTCache->destroy();
	prt::removeLogHandler(mLogHandler.get());
}

OP_ERROR SOP_PRT::cookMySop(OP_Context &context) {
	LOG_DBG << "cookMySop";

	if (!handleParams(context))
		return UT_ERROR_ABORT;

	if (lockInputs(context) >= UT_ERROR_ABORT) {
		LOG_DBG << "lockInputs error";
		return error();
	}

	duplicateSource(0, context);

	if (error() < UT_ERROR_ABORT && cookInputGroups(context) < UT_ERROR_ABORT) {
		UT_AutoInterrupt progress("Generating CityEngine Geometry...");

		InitialShapeGenerator isc(gdp, mInitialShapeContext);
		gdp->clearAndDestroy();
		{
			HoudiniGeometry hg(gdp);
			// TODO: add occlusion
			InitialShapeNOPtrVector& initialShapes = isc.getInitialShapes();
			prt::Status stat = prt::generate(
					initialShapes.data(), initialShapes.size(), 0,
					mAllEncoders.data(), mAllEncoders.size(), mAllEncoderOptions.data(),
					&hg, mPRTCache, nullptr
			);
			if(stat != prt::STATUS_OK) {
				LOG_ERR << "prt::generate() failed with status: '" << prt::getStatusDescription(stat) << "' (" << stat << ")";
			}
		}
		select(GU_SPrimitive);
	}

	unlockInputs();

	LOG_DBG << "cookMySop done";
	return error();
}

namespace {

void getParamDef(
		const RuleFileInfoPtr& info,
		TypedParamNames& createdParams,
		std::ostream& defStream
) {
	for(size_t i = 0; i < info->getNumAttributes(); i++) {
		if (info->getAttribute(i)->getNumParameters() != 0)
			continue;

		const wchar_t* attrName = info->getAttribute(i)->getName();
		std::string nAttrName = utils::toOSNarrowFromUTF16(attrName);
		nAttrName = nAttrName.substr(8); // TODO: better way to remove style for now...

		defStream << "parm {" << "\n";
		defStream << "    name  \"" << nAttrName << "\"" << "\n";
		defStream << "    label \"" << nAttrName << "\"\n";

		switch(info->getAttribute(i)->getReturnType()) {
		case prt::AAT_BOOL: {
			defStream << "    type    integer\n";
			break;
		}
		case prt::AAT_FLOAT: {
			defStream << "    type    float\n";
			// TODO: handle @RANGE annotation:	parmDef << "range   { 0 10 }\n";
			createdParams[prt::Attributable::PT_FLOAT].push_back(nAttrName);
			break;
		}
		case prt::AAT_STR: {
			defStream << "    type    string\n";
			createdParams[prt::Attributable::PT_STRING].push_back(nAttrName);
			break;
		}
		default:
			break;
		}

		defStream << "    size    1\n";
		defStream << "    export  none\n";
		defStream << "}\n";
	}
}

namespace UnitQuad {
const double 	vertices[]				= { 0, 0, 0,  0, 0, 1,  1, 0, 1,  1, 0, 0 };
const size_t 	vertexCount				= 12;
const uint32_t	indices[]				= { 0, 1, 2, 3 };
const size_t 	indexCount				= 4;
const uint32_t 	faceCounts[]			= { 4 };
const size_t 	faceCountsCount			= 1;
}

} // anonymous namespace

bool SOP_PRT::handleParams(OP_Context &context) {
	LOG_DBG << "handleParams begin";
	fpreal now = context.getTime();

	// -- shape classifier attr name
	evalString(mInitialShapeContext.mShapeClsAttrName, NODE_PARAM_SHAPE_CLS_ATTR.getToken(), 0, now);

	// -- shape classifier attr type
	int shapeClsAttrTypeChoice = evalInt(NODE_PARAM_SHAPE_CLS_TYPE.getToken(), 0, now);
	if (shapeClsAttrTypeChoice == 0)
		mInitialShapeContext.mShapeClsType = GA_STORECLASS_STRING;
	else if (shapeClsAttrTypeChoice == 1)
		mInitialShapeContext.mShapeClsType = GA_STORECLASS_INT;
	else if (shapeClsAttrTypeChoice == 2)
		mInitialShapeContext.mShapeClsType = GA_STORECLASS_FLOAT;

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
	prt::LogLevel ll = static_cast<prt::LogLevel>(evalInt(NODE_PARAM_LOG.getToken(), 0, now));
	mLogHandler->setLevel(ll);
	mLogHandler->setName(utils::toUTF16FromOSNarrow(getName().toStdString()));

	LOG_DBG << "handleParams done.";
	return true;
}

namespace {

void getCGBs(const ResolveMapPtr& rm, std::vector<std::pair<std::wstring,std::wstring>>& cgbs) {
	static const wchar_t*	PROJECT		= L"";
	static const wchar_t*	PATTERN		= L"*.cgb";
	static const size_t		START_SIZE	= 16 * 1024;

	size_t resultSize = START_SIZE;
	wchar_t* result = new wchar_t[resultSize];
	rm->searchKey(PROJECT, PATTERN, result, &resultSize);
	if (resultSize >= START_SIZE) {
		delete[] result;
		result = new wchar_t[resultSize];
		rm->searchKey(PROJECT, PATTERN, result, &resultSize);
	}
	std::wstring cgbList(result);
	delete[] result;
	LOG_DBG << "   cgbList = '" << cgbList << "'";

	std::vector<std::wstring> tok;
	boost::split(tok, cgbList, boost::is_any_of(L";"), boost::algorithm::token_compress_on);
	for(const std::wstring& t: tok) {
		if (t.empty()) continue;
		LOG_DBG << "token: '" << t << "'";
		const wchar_t* s = rm->getString(t.c_str());
		if (s != nullptr) {
			cgbs.emplace_back(t, s);
			LOG_DBG << L"got cgb: " << cgbs.back().first << L" => " << cgbs.back().second;
		}
	}
}

void getDefaultRuleAttributeValues(
		prt::AttributeMapBuilder* amb,
		prt::Cache* cache,
		const ResolveMapPtr& resolveMap,
		const std::wstring& cgbKey,
		const std::wstring& startRule
) {
	HoudiniGeometry hg(nullptr, amb);
	const prt::AttributeMap* emptyAttrMap = amb->createAttributeMapAndReset();

	prt::InitialShapeBuilder* isb = prt::InitialShapeBuilder::create();
	isb->setGeometry(UnitQuad::vertices, UnitQuad::vertexCount, UnitQuad::indices, UnitQuad::indexCount, UnitQuad::faceCounts, UnitQuad::faceCountsCount);
	isb->setAttributes(cgbKey.c_str(), startRule.c_str(), 666, L"temp", emptyAttrMap, resolveMap.get());

	prt::Status status = prt::STATUS_UNSPECIFIED_ERROR;
	const prt::InitialShape* is = isb->createInitialShapeAndReset(&status);
	isb->destroy();
	const prt::InitialShape* iss[1] = { is };

	const prt::EncoderInfo* encInfo = prt::createEncoderInfo(ENCODER_ID_CGA_EVALATTR);
	const prt::AttributeMap* encOpts = nullptr;
	encInfo->createValidatedOptionsAndStates(nullptr, &encOpts);
	encInfo->destroy();

	const wchar_t* encs[1] = { ENCODER_ID_CGA_EVALATTR };
	const prt::AttributeMap* encsOpts[1] = { encOpts };

	prt::Status stat = prt::generate(iss, 1, nullptr, encs, 1, encsOpts, &hg, cache, nullptr, nullptr);
	if(stat != prt::STATUS_OK) {
		LOG_ERR << "prt::generate() failed with status: '" << prt::getStatusDescription(stat) << "' (" << stat << ")";
	}

	encOpts->destroy();
	is->destroy();
	emptyAttrMap->destroy();
}

} // anonymous namespace

bool SOP_PRT::updateRulePackage(const boost::filesystem::path& nextRPK, fpreal time) {
	if (nextRPK == mInitialShapeContext.mRPK)
		return true;
	if (!boost::filesystem::exists(nextRPK))
		return false;

	std::wstring nextRPKURI = utils::toFileURI(nextRPK);
	LOG_DBG << L"nextRPKURI = " << nextRPKURI;

	// rebuild assets map
	prt::Status status = prt::STATUS_UNSPECIFIED_ERROR;
	boost::filesystem::path unpackPath = boost::filesystem::temp_directory_path();
	ResolveMapPtr nextResolveMap(prt::createResolveMap(nextRPKURI.c_str(), unpackPath.wstring().c_str(), &status));
	if (!nextResolveMap || status != prt::STATUS_OK) {
		LOG_ERR << "failed to create resolve map from '" << nextRPKURI << "', aborting.";
		return false;
	}

	// get first rule file
	std::vector<std::pair<std::wstring,std::wstring>> cgbs; // key -> uri
	getCGBs(nextResolveMap, cgbs);
	if (cgbs.empty()) {
		LOG_ERR << "no rule files found in rule package";
		return false;
	}
	std::wstring cgbKey = cgbs.front().first;
	std::wstring cgbURI = cgbs.front().second;
	LOG_DBG << "cgbKey = " << cgbKey;
	LOG_DBG << "cgbURI = " << cgbURI;

	status = prt::STATUS_UNSPECIFIED_ERROR;
	RuleFileInfoPtr info(prt::createRuleFileInfo(cgbURI.c_str(), mPRTCache, &status));
	if (!info || status != prt::STATUS_OK) {
		LOG_ERR << "failed to get rule file info";
		return false;
	}

	// get first rule
	if (info->getNumRules() == 0) {
		LOG_ERR << "rule file does not contain any rules";
		return false;
	}
	const prt::RuleFileInfo::Entry* re = info->getRule(0);
	std::wstring fqStartRule = re->getName();
	std::vector<std::wstring> startRuleComponents;
	boost::split(startRuleComponents, fqStartRule, boost::is_any_of(L"$"));
	LOG_DBG << "first start rule: " << fqStartRule;

	// update node
	mInitialShapeContext.mRPK = nextRPK;
	mInitialShapeContext.mAssetsMap.swap(nextResolveMap);
	mPRTCache->flushAll();
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
	LOG_DBG << "updateRulePackage done: mRuleFile = " << mInitialShapeContext.mRuleFile << ", mStyle = " << mInitialShapeContext.mStyle << ", mStartRule = " << mInitialShapeContext.mStartRule;

	// regenerate spare params for rule attributes
	createSpareParams(info, mInitialShapeContext.mRuleFile, fqStartRule, time);

	return true;
}

void SOP_PRT::createSpareParams(
		const RuleFileInfoPtr& info,
		const std::wstring& cgbKey,
		const std::wstring& fqStartRule,
		fpreal time
) {
	// eval rule attribute values
	prt::AttributeMapBuilder* amb = prt::AttributeMapBuilder::create();
	getDefaultRuleAttributeValues(amb, mPRTCache, mInitialShapeContext.mAssetsMap, cgbKey, fqStartRule);
	const prt::AttributeMap* defAttrVals = amb->createAttributeMap();
	amb->destroy();
	LOG_DBG << "defAttrVals = " << utils::objectToXML(defAttrVals);

	// build spare param definition
	mInitialShapeContext.mActiveParams.clear();
	std::ostringstream paramDef;
	getParamDef(info, mInitialShapeContext.mActiveParams, paramDef);
	std::string s = paramDef.str();
	LOG_DBG << "INPUT PARMS" << s;

	OP_Director* director = OPgetDirector();
	director->deleteAllNodeSpareParms(this);

	UT_IStream istr(&s[0], s.size(), UT_ISTREAM_ASCII);
	UT_String errors;
	director->loadNodeSpareParms(this, istr, errors);

	// setup spare params with rule defaults
	size_t keyCount = 0;
	const wchar_t* const* cKeys = defAttrVals->getKeys(&keyCount);
	for (size_t k = 0; k < keyCount; k++) {
		const wchar_t* key = cKeys[k];
		std::string nKey = utils::toOSNarrowFromUTF16(key);
		nKey = nKey.substr(8); // TODO: better way to remove style for now...
		switch (defAttrVals->getType(key)) {
		case prt::AttributeMap::PT_FLOAT: {
			setFloat(nKey.c_str(), 0, time, defAttrVals->getFloat(key));
			break;
		}
		case prt::AttributeMap::PT_STRING: {
			UT_String val(utils::toOSNarrowFromUTF16(defAttrVals->getString(key)));
			setString(val, CH_STRING_LITERAL, nKey.c_str(), 0, time);
			break;
		}
		default: {
			LOG_WRN << "attribute " << nKey << ": type not handled";
			break;
		}
		}
	}

#if 0
	std::ostringstream ostr;
	director->saveNodeSpareParms(this, true, ostr);
	LOG_DBG << " SAVED PARMS" << ostr.str();
#endif

	defAttrVals->destroy();
}

bool SOP_PRT::updateParmsFlags() {
	for (std::string& p: mInitialShapeContext.mActiveParams[prt::AttributeMap::PT_FLOAT]) {
		double v = evalFloat(p.c_str(), 0, 0.0); // TODO: time
		std::wstring wp = utils::toUTF16FromOSNarrow(p);
		mInitialShapeContext.mAttributeSource->setFloat(wp.c_str(), v);
	}
	for (std::string& p: mInitialShapeContext.mActiveParams[prt::AttributeMap::PT_STRING]) {
		UT_String v;
		evalString(v, p.c_str(), 0, 0.0); // TODO: time
		std::wstring wp = utils::toUTF16FromOSNarrow(p);
		std::wstring wv = utils::toUTF16FromOSNarrow(v.toStdString());
		mInitialShapeContext.mAttributeSource->setString(wp.c_str(), wv.c_str());
	}
	// TODO: bool

	forceRecook();

	return SOP_Node::updateParmsFlags();
}


namespace {

typedef std::vector<std::pair<std::string,std::string>> StringPairVector;

bool compareSecond (const StringPairVector::value_type& a, const StringPairVector::value_type& b) { return ( a.second < b.second ); }

}

void SOP_PRT::buildStartRuleMenu(void* data, PRM_Name* theMenu, int theMaxSize, const PRM_SpareData*, const PRM_Parm*) {
		SOP_PRT* node = static_cast<SOP_PRT*>(data);
		LOG_DBG << "buildStartRuleMenu";
		LOG_DBG << "   mRPK = " << node->mInitialShapeContext.mRPK;
		LOG_DBG << "   mRuleFile = " << node->mInitialShapeContext.mRuleFile;

		if (node->mInitialShapeContext.mAssetsMap == nullptr || node->mInitialShapeContext.mRPK.empty() || node->mInitialShapeContext.mRuleFile.empty()) {
			theMenu[0].setToken(0);
			return;
		}

		const wchar_t* cgbURI = node->mInitialShapeContext.mAssetsMap->getString(node->mInitialShapeContext.mRuleFile.c_str());
		if (cgbURI == nullptr) {
			LOG_ERR << L"failed to resolve rule file '" << node->mInitialShapeContext.mRuleFile << "', aborting.";
			return;
		}

		prt::Status status = prt::STATUS_UNSPECIFIED_ERROR;
		const prt::RuleFileInfo* rfi = prt::createRuleFileInfo(cgbURI, nullptr, &status);
		if (status == prt::STATUS_OK) {
			StringPairVector startRules, rules;
			for (size_t ri = 0; ri < rfi->getNumRules() ; ri++) {
				const prt::RuleFileInfo::Entry* re = rfi->getRule(ri);
				std::string rn = utils::toOSNarrowFromUTF16(re->getName());
				std::vector<std::string> tok;
				boost::split(tok, rn, boost::is_any_of("$"));

				bool hasStartRuleAnnotation = false;
				for (size_t ai = 0; ai < re->getNumAnnotations(); ai++) {
					if (std::wcscmp(re->getAnnotation(ai)->getName(), L"@StartRule") == 0) {
						hasStartRuleAnnotation = true;
						break;
					}
				}

				if (hasStartRuleAnnotation)
					startRules.emplace_back(tok[1], tok[1] + " (@StartRule)");
				else
					rules.emplace_back(tok[1], tok[1]);
			}

			std::sort(startRules.begin(), startRules.end(), compareSecond);
			std::sort(rules.begin(), rules.end(), compareSecond);
			rules.reserve(rules.size() + startRules.size());
			rules.insert(rules.begin(), startRules.begin(), startRules.end());

			const size_t limit = std::min<size_t>(rules.size(), theMaxSize);
			for (size_t ri = 0; ri < limit; ri++) {
				theMenu[ri].setToken(rules[ri].first.c_str());
				theMenu[ri].setLabel(rules[ri].second.c_str());
			}
			theMenu[limit].setToken(0); // need a null terminator
			rfi->destroy();
		}
}

void SOP_PRT::buildRuleFileMenu(void* data, PRM_Name* theMenu, int theMaxSize, const PRM_SpareData*, const PRM_Parm*) {
	SOP_PRT* node = static_cast<SOP_PRT*>(data);
	LOG_DBG << "buildRuleFileMenu";

	if (!node->mInitialShapeContext.mAssetsMap || node->mInitialShapeContext.mRPK.empty()) {
		theMenu[0].setToken(0);
		return;
	}

	std::vector<std::pair<std::wstring,std::wstring>> cgbs; // key -> uri
	getCGBs(node->mInitialShapeContext.mAssetsMap, cgbs);

	const size_t limit = std::min<size_t>(cgbs.size(), theMaxSize);
	for (size_t ri = 0; ri < limit; ri++) {
		std::string tok = utils::toOSNarrowFromUTF16(cgbs[ri].first);
		std::string lbl = utils::toOSNarrowFromUTF16(cgbs[ri].first); // TODO
		theMenu[ri].setToken(tok.c_str());
		theMenu[ri].setLabel(lbl.c_str());
	}
	theMenu[limit].setToken(0); // need a null terminator
}


} // namespace p4h
