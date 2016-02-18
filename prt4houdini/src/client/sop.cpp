#include "client/sop.h"
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
#include "boost/dynamic_bitset.hpp"
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


typedef std::vector<const prt::InitialShape*> InitialShapePtrVector;
typedef std::vector<const prt::AttributeMap*> AttributeMapPtrVector;


struct InitialShapeContext {
	InitialShapeContext(OP_Context& ctx, GU_Detail& detail) : mContext(ctx) {
		LOG_DBG << "-- creating intial shape context";

		//mAMB = prt::AttributeMapBuilder::create();
		mISB = prt::InitialShapeBuilder::create();

		// collect all primitive attributes
		// TODO: GA_AttributeDict is deprecated, use GA_AttributeSet
		for (GA_AttributeDict::iterator it = detail.getAttributeDict(GA_ATTRIB_PRIMITIVE).begin(GA_SCOPE_PUBLIC); !it.atEnd(); ++it) {
			mPrimitiveAttributes.push_back(GA_ROAttributeRef(it.attrib()));
			LOG_DBG << "    prim attr: " << mPrimitiveAttributes.back()->getName();
		}
		LOG_DBG << "    got primitive attribute count = " << mPrimitiveAttributes.size();

	}

	~InitialShapeContext() {
		BOOST_FOREACH(const prt::InitialShape* is, mInitialShapes) {
			is->destroy();
		}
		mISB->destroy();
		BOOST_FOREACH(const prt::AttributeMap* am, mInitialShapeAttributes) {
			am->destroy();
		}
		//mAMB->destroy();
	}

	OP_Context& mContext;
	std::vector<GA_ROAttributeRef> mPrimitiveAttributes;

	prt::InitialShapeBuilder* mISB;
	InitialShapePtrVector mInitialShapes;
	//prt::AttributeMapBuilder* mAMB;
	AttributeMapPtrVector mInitialShapeAttributes;
};

const char* NODE_PARAM_RPK			= "rpk";
const char* NODE_PARAM_RULE_FILE	= "ruleFile";
const char* NODE_PARAM_STYLE		= "style";
const char* NODE_PARAM_START_RULE	= "startRule";
const char* NODE_PARAM_SEED			= "seed";
const char* NODE_PARAM_LOG			= "logLevel";

PRM_Name NODE_PARAM_NAMES[] = {
		PRM_Name(NODE_PARAM_RPK,		"Rule Package"),
		PRM_Name(NODE_PARAM_RULE_FILE,	"Rule File"),
		PRM_Name(NODE_PARAM_STYLE,		"Style"),
		PRM_Name(NODE_PARAM_START_RULE,	"Start Rule"),
		PRM_Name(NODE_PARAM_SEED,		"Rnd Seed"),
		PRM_Name(NODE_PARAM_LOG,		"Log Level")
};

PRM_Default rpkDefault(0, "$HIP/$F.rpk");
PRM_Default startRuleDefault(0, "Start");
PRM_Default ruleFileDefault(0, "<none>");
PRM_Default logDefault(0, "ERROR");

PRM_ChoiceList ruleFileMenu(static_cast<PRM_ChoiceListType>(PRM_CHOICELIST_EXCLUSIVE | PRM_CHOICELIST_REPLACE), &p4h::SOP_PRT::buildRuleFileMenu);
PRM_ChoiceList startRuleMenu(static_cast<PRM_ChoiceListType>(PRM_CHOICELIST_EXCLUSIVE | PRM_CHOICELIST_REPLACE), &p4h::SOP_PRT::buildStartRuleMenu);

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

PRM_Template NODE_PARAM_TEMPLATES[] = {
		PRM_Template(PRM_FILE,		1, &NODE_PARAM_NAMES[0],	&rpkDefault, 0, 0, 0, &PRM_SpareData::fileChooserModeRead),
		PRM_Template(PRM_STRING,	1, &NODE_PARAM_NAMES[1],	PRMoneDefaults, &ruleFileMenu),
		PRM_Template(PRM_STRING,	1, &NODE_PARAM_NAMES[2],	PRMoneDefaults),
		PRM_Template(PRM_STRING,	1, &NODE_PARAM_NAMES[3],	PRMoneDefaults, &startRuleMenu),
		PRM_Template(PRM_INT,		1, &NODE_PARAM_NAMES[4],	PRMoneDefaults),
		PRM_Template((PRM_Type)PRM_ORD, PRM_Template::PRM_EXPORT_MAX, 1, &NODE_PARAM_NAMES[5], &logDefault, &logMenu),
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
, mLogHandler(new log::LogHandler(utils::toUTF16FromOSNarrow(name), prt::LOG_ERROR))
, mPRTCache(prt::CacheObject::create(prt::CacheObject::CACHE_TYPE_DEFAULT))
, mAttributeSource(prt::AttributeMapBuilder::create())
{
	prt::addLogHandler(mLogHandler.get());

	prt::AttributeMapBuilder* optionsBuilder = prt::AttributeMapBuilder::create();

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

	optionsBuilder->destroy();

#ifdef WIN32
	mAllEncoders = boost::assign::list_of(ENCODER_ID_HOUDINI)(ENCODER_ID_CGA_ERROR)(ENCODER_ID_CGA_PRINT);
	mAllEncoderOptions = boost::assign::list_of(mHoudiniEncoderOptions)(mCGAErrorOptions)(mCGAPrintOptions);
#else
	mAllEncoders = { ENCODER_ID_HOUDINI, ENCODER_ID_CGA_ERROR, ENCODER_ID_CGA_PRINT };
	mAllEncoderOptions = { mHoudiniEncoderOptions, mCGAErrorOptions, mCGAPrintOptions };
#endif
}

SOP_PRT::~SOP_PRT() {
	mAttributeSource->destroy();

	mAssetsMap.reset();

	mHoudiniEncoderOptions->destroy();
	mCGAErrorOptions->destroy();
	mCGAPrintOptions->destroy();

	mPRTCache->destroy();

	prt::removeLogHandler(mLogHandler.get());
}

void SOP_PRT::createInitialShape(const GA_Group* group, void* ctx) {
	static const bool DBG = false;

	InitialShapeContext* isc = static_cast<InitialShapeContext*>(ctx);
	if (DBG) {
		LOG_DBG << "-- creating initial shape geo from group " << group->getName(); // << ": vtx = " << vtx << ", idx = " << idx << ", faceCounts = " << faceCounts;
	}

	// convert geometry
	std::vector<double> vtx;
	std::vector<uint32_t> idx, faceCounts;

	GA_Offset ptoff;
	GA_FOR_ALL_PTOFF(gdp, ptoff) {
		UT_Vector3 p = gdp->getPos3(ptoff);
		vtx.push_back(static_cast<double>(p.x()));
		vtx.push_back(static_cast<double>(p.y()));
		vtx.push_back(static_cast<double>(p.z()));
	}

	// loop over all polygons in the primitive group
	// in case of multipoly initial shapes, attr on first one wins
	boost::dynamic_bitset<> setAttributes(isc->mPrimitiveAttributes.size());
	GA_Primitive* prim = nullptr;
	GA_FOR_ALL_GROUP_PRIMITIVES(gdp, static_cast<const GA_PrimitiveGroup*>(group), prim) {
		GU_PrimPoly* face = static_cast<GU_PrimPoly*>(prim);
		for(GA_Size i = face->getVertexCount()-1; i >= 0 ; i--) {
			GA_Offset off = face->getPointOffset(i);
			idx.push_back(static_cast<uint32_t>(off));
		}
		faceCounts.push_back(static_cast<uint32_t>(face->getVertexCount()));

		// extract primitive attributes
		for (size_t ai = 0; ai < isc->mPrimitiveAttributes.size(); ai++) {
			if (!setAttributes[ai]) {
				const GA_ROAttributeRef& ar = isc->mPrimitiveAttributes[ai];
				if (ar.isFloat() || ar.isInt()) {
					double v = prim->getValue<double>(ar);
					if (DBG) LOG_DBG << "   setting float attrib " << ar->getName() << " = " << v;
					std::wstring wn = utils::toUTF16FromOSNarrow(ar->getName());
					mAttributeSource->setFloat(wn.c_str(), v);
					setAttributes.set(ai);
				} else if (ar.isString()) {
					const char* v = prim->getString(ar);
					if (DBG) LOG_DBG << "   setting string attrib " << ar->getName() << " = " << v;
					std::wstring wn = utils::toUTF16FromOSNarrow(ar->getName());
					std::wstring wv = utils::toUTF16FromOSNarrow(v);
					mAttributeSource->setString(wn.c_str(), wv.c_str());
					setAttributes.set(ai);
				}
			}
		}
	}

	const prt::AttributeMap* initialShapeAttrs = mAttributeSource->createAttributeMap();

	isc->mISB->setGeometry(vtx.data(), vtx.size(), idx.data(), idx.size(), faceCounts.data(), faceCounts.size());

	std::wstring shapeName = utils::toUTF16FromOSNarrow(group->getName().toStdString());
	std::wstring startRule = mStyle + L"$" + mStartRule;
	isc->mISB->setAttributes(
			mRuleFile.c_str(),
			startRule.c_str(),
			mSeed,
			shapeName.c_str(),
			initialShapeAttrs,
			mAssetsMap.get()
	);

	prt::Status status = prt::STATUS_UNSPECIFIED_ERROR;
	const prt::InitialShape* initialShape = isc->mISB->createInitialShapeAndReset(&status);
	if (status != prt::STATUS_OK) {
		LOG_WRN << "ignored input group '" << group->getName() << "': " << prt::getStatusDescription(status);
		initialShapeAttrs->destroy();
		return;
	}

	if (DBG) LOG_DBG << p4h::utils::objectToXML(initialShape);

	isc->mInitialShapes.push_back(initialShape);
	isc->mInitialShapeAttributes.push_back(initialShapeAttrs);
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

		InitialShapeContext isc(context, *gdp);
		const char* pattern = "*";
		GroupOperation createInitialShapeOp = static_cast<GroupOperation>(&SOP_PRT::createInitialShape);
		forEachGroupMatchingMask(pattern, createInitialShapeOp, static_cast<void*>(&isc), GA_GROUP_PRIMITIVE, gdp);

		gdp->clearAndDestroy();

		HoudiniGeometry hg(gdp);
		{
			prt::Status stat = prt::generate(
					&isc.mInitialShapes[0], isc.mInitialShapes.size(), 0,
					&mAllEncoders[0], mAllEncoders.size(), &mAllEncoderOptions[0],
					&hg, mPRTCache, 0
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
		SOP_PRT::TypedParamNames& createdParams,
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

	// -- rule package
	UT_String utNextRPKStr;
	evalString(utNextRPKStr, NODE_PARAM_RPK, 0, now);
	boost::filesystem::path nextRPK(utNextRPKStr.toStdString());
	if (!updateRulePackage(nextRPK, now)) {
		UT_WorkBuffer           buf;
        buf.sprintf("Given file \"%s\" is not a valid rule package file",
                     utNextRPKStr.buffer());
		addError(SOP_MESSAGE,buf.buffer());
		return false;
	}

	// -- rule file
	UT_String utRuleFile;
	evalString(utRuleFile, NODE_PARAM_RULE_FILE, 0, now);
	mRuleFile = utils::toUTF16FromOSNarrow(utRuleFile.toStdString());
	LOG_DBG << L"got rule file: " << mRuleFile;

	// -- style
	UT_String utStyle;
	evalString(utStyle, NODE_PARAM_STYLE, 0, now);
	mStyle = utils::toUTF16FromOSNarrow(utStyle.toStdString());

	// -- start rule
	UT_String utStartRule;
	evalString(utStartRule, NODE_PARAM_START_RULE, 0, now);
	mStartRule = utils::toUTF16FromOSNarrow(utStartRule.toStdString());

	// -- random seed
	mSeed = evalInt(NODE_PARAM_SEED, 0, now);

	// -- logger
	prt::LogLevel ll = static_cast<prt::LogLevel>(evalInt(NODE_PARAM_LOG, 0, now));
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
	if (nextRPK == mRPK)
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
	mRPK = nextRPK;
	mAssetsMap.swap(nextResolveMap);
	mPRTCache->flushAll();
	{
		mRuleFile = cgbKey;
		UT_String val(utils::toOSNarrowFromUTF16(mRuleFile));
		setString(val, CH_STRING_LITERAL, NODE_PARAM_RULE_FILE, 0, time);
	}
	{
		mStyle = startRuleComponents[0];
		UT_String val(utils::toOSNarrowFromUTF16(mStyle));
		setString(val, CH_STRING_LITERAL, NODE_PARAM_STYLE, 0, time);
	}
	{
		mStartRule = startRuleComponents[1];
		UT_String val(utils::toOSNarrowFromUTF16(mStartRule));
		setString(val, CH_STRING_LITERAL, NODE_PARAM_START_RULE, 0, time);
	}
	{
		mSeed = 0;
		setInt(NODE_PARAM_SEED, 0, time, mSeed);
	}
	LOG_DBG << "updateRulePackage done: mRuleFile = " << mRuleFile << ", mStyle = " << mStyle << ", mStartRule = " << mStartRule;

	// regenerate spare params for rule attributes
	createSpareParams(info, mRuleFile, fqStartRule, time);

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
	getDefaultRuleAttributeValues(amb, mPRTCache, mAssetsMap, cgbKey, fqStartRule);
	const prt::AttributeMap* defAttrVals = amb->createAttributeMap();
	amb->destroy();
	LOG_DBG << "defAttrVals = " << utils::objectToXML(defAttrVals);

	// build spare param definition
	mActiveParams.clear();
	std::ostringstream paramDef;
	getParamDef(info, mActiveParams, paramDef);
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
	for (std::string& p: mActiveParams[prt::AttributeMap::PT_FLOAT]) {
		double v = evalFloat(p.c_str(), 0, 0.0); // TODO: time
		std::wstring wp = utils::toUTF16FromOSNarrow(p);
		mAttributeSource->setFloat(wp.c_str(), v);
	}
	for (std::string& p: mActiveParams[prt::AttributeMap::PT_STRING]) {
		UT_String v;
		evalString(v, p.c_str(), 0, 0.0); // TODO: time
		std::wstring wp = utils::toUTF16FromOSNarrow(p);
		std::wstring wv = utils::toUTF16FromOSNarrow(v.toStdString());
		mAttributeSource->setString(wp.c_str(), wv.c_str());
	}

	forceRecook();

	bool changed = SOP_Node::updateParmsFlags();
	return changed;
}


namespace {

typedef std::vector<std::pair<std::string,std::string>> StringPairVector;

bool compareSecond (const StringPairVector::value_type& a, const StringPairVector::value_type& b) { return ( a.second < b.second ); }

}

void SOP_PRT::buildStartRuleMenu(void* data, PRM_Name* theMenu, int theMaxSize, const PRM_SpareData*, const PRM_Parm*) {
		SOP_PRT* node = static_cast<SOP_PRT*>(data);
		LOG_DBG << "buildStartRuleMenu";
		LOG_DBG << "   mRPK = " << node->mRPK;
		LOG_DBG << "   mRuleFile = " << node->mRuleFile;

		if (node->mAssetsMap == nullptr || node->mRPK.empty() || node->mRuleFile.empty()) {
			theMenu[0].setToken(0);
			return;
		}

		const wchar_t* cgbURI = node->mAssetsMap->getString(node->mRuleFile.c_str());
		if (cgbURI == nullptr) {
			LOG_ERR << L"failed to resolve rule file '" << node->mRuleFile << "', aborting.";
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

	if (!node->mAssetsMap || node->mRPK.empty()) {
		theMenu[0].setToken(0);
		return;
	}

	std::vector<std::pair<std::wstring,std::wstring>> cgbs; // key -> uri
	getCGBs(node->mAssetsMap, cgbs);

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
