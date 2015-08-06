#include "SOP_prt4houdini.h"
#include "logging.h"
#include "utils.h"

#include "HoudiniCallbacks.h"

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
#ifndef WIN32
#	pragma GCC diagnostic pop
#endif

#include "boost/foreach.hpp"
#include "boost/filesystem.hpp"
#include "boost/dynamic_bitset.hpp"
#include "boost/algorithm/string/replace.hpp"
#include "boost/assign.hpp"

#include <vector>


namespace {

// some file/directory name definitions
const char*		PRT_LIB_SUBDIR			= "prtlib";
const char*		FILE_FLEXNET_LIB		= "flexnet_prt";
const wchar_t*	FILE_CGA_ERROR			= L"CGAErrors.txt";
const wchar_t*	FILE_CGA_PRINT			= L"CGAPrint.txt";

// some encoder IDs
const wchar_t*	ENCODER_ID_CGA_ERROR	= L"com.esri.prt.core.CGAErrorEncoder";
const wchar_t*	ENCODER_ID_CGA_PRINT	= L"com.esri.prt.core.CGAPrintEncoder";
const wchar_t*	ENCODER_ID_HOUDINI		= L"HoudiniEncoder";

// global objects with the same lifetime as the whole
const prt::Object* prtLicHandle = 0;
prt::ConsoleLogHandler*	prtConsoleLog = 0;

void dsoExit(void*) {
	if (prtLicHandle)
		prtLicHandle->destroy(); // prt shutdown
	if (prtConsoleLog)
		prt::removeLogHandler(prtConsoleLog);
}

const bool DBG = false;

class HoudiniGeometry : public HoudiniCallbacks {
public:
	HoudiniGeometry(GU_Detail* gdp) : mDetail(gdp) { }

protected:
	virtual void setVertices(double* vtx, size_t size) {
		mPoints.reserve(size / 3);
		for (size_t pi = 0; pi < size; pi += 3)
			mPoints.push_back(UT_Vector3(vtx[pi], vtx[pi+1], vtx[pi+2]));
	}

	virtual void setNormals(double* nrm, size_t size) {
		// TODO
	}

	virtual void setUVs(float* u, float* v, size_t size) {
		// TODO
	}

	virtual void setFaces(int* counts, size_t countsSize, int* connects, size_t connectsSize, int* uvCounts, size_t uvCountsSize, int* uvConnects, size_t uvConnectsSize) {
		for (size_t ci = 0; ci < countsSize; ci++)
			mPolyCounts.append(counts[ci]);
		mIndices.reserve(connectsSize);
		mIndices.insert(mIndices.end(), connects, connects+connectsSize);
	}

	virtual void matSetColor(int start, int count, float r, float g, float b) {
		// TODO
	}

	virtual void matSetDiffuseTexture(int start, int count, const wchar_t* tex) {
		// TODO
	}

	virtual void createMesh(const wchar_t* name) {
		std::string nname = prt4hdn::utils::toOSNarrowFromUTF16(name);
		mCurGroup = static_cast<GA_ElementGroup*>(mDetail->getElementGroupTable(GA_ATTRIB_PRIMITIVE).newGroup(nname.c_str(), false));
		if (DBG) LOG_DBG << "createMesh: " << nname;
	}

	virtual void finishMesh() {
		GA_IndexMap::Marker marker(mDetail->getPrimitiveMap());
		GU_PrimPoly::buildBlock(mDetail, &mPoints[0], mPoints.size(), mPolyCounts, &mIndices[0]);
		mCurGroup->addRange(marker.getRange());
		mPolyCounts.clear();
		mIndices.clear();
		mPoints.clear();
		if (DBG) LOG_DBG << "finishMesh";
	}

	virtual prt::Status generateError(size_t isIndex, prt::Status status, const wchar_t* message) {
		return prt::STATUS_OK;
	}
	virtual prt::Status assetError(size_t isIndex, prt::CGAErrorLevel level, const wchar_t* key, const wchar_t* uri, const wchar_t* message) {
		return prt::STATUS_OK;
	}
	virtual prt::Status cgaError(size_t isIndex, int32_t shapeID, prt::CGAErrorLevel level, int32_t methodId, int32_t pc, const wchar_t* message) {
		return prt::STATUS_OK;
	}
	virtual prt::Status cgaPrint(size_t isIndex, int32_t shapeID, const wchar_t* txt) {
		return prt::STATUS_OK;
	}
	virtual prt::Status cgaReportBool(size_t isIndex, int32_t shapeID, const wchar_t* key, bool value) {
		return prt::STATUS_OK;
	}
	virtual prt::Status cgaReportFloat(size_t isIndex, int32_t shapeID, const wchar_t* key, double value) {
		return prt::STATUS_OK;
	}
	virtual prt::Status cgaReportString(size_t isIndex, int32_t shapeID, const wchar_t* key, const wchar_t* value) {\
		return prt::STATUS_OK;
	}
	virtual prt::Status attrBool(size_t isIndex, int32_t shapeID, const wchar_t* key, bool value) {
		return prt::STATUS_OK;
	}
	virtual prt::Status attrFloat(size_t isIndex, int32_t shapeID, const wchar_t* key, double value) {
		return prt::STATUS_OK;
	}
	virtual prt::Status attrString(size_t isIndex, int32_t shapeID, const wchar_t* key, const wchar_t* value) {
		return prt::STATUS_OK;
	}

private:
	GU_Detail* mDetail;
	GA_ElementGroup* mCurGroup;
	std::vector<UT_Vector3> mPoints;
	std::vector<int> mIndices;
	GEO_PolyCounts mPolyCounts;
};

typedef std::vector<const prt::InitialShape*> InitialShapePtrVector;
typedef std::vector<const prt::AttributeMap*> AttributeMapPtrVector;

struct InitialShapeContext {
	InitialShapeContext(OP_Context& ctx, GU_Detail& detail) : mContext(ctx) {
		//mAMB = prt::AttributeMapBuilder::create();
		mISB = prt::InitialShapeBuilder::create();

		// collect all primitive attributes
		for (GA_AttributeDict::iterator it = detail.getAttributeDict(GA_ATTRIB_PRIMITIVE).begin(GA_SCOPE_PUBLIC); !it.atEnd(); ++it) {
			mPrimitiveAttributes.push_back(GA_ROAttributeRef(it.attrib()));
		}
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

PRM_Name NODE_PARAM_NAMES[] = {
		PRM_Name(NODE_PARAM_RPK,		"Rule Package"),
		PRM_Name(NODE_PARAM_RULE_FILE,	"Rule File"),
		PRM_Name(NODE_PARAM_STYLE,		"Style"),
		PRM_Name(NODE_PARAM_START_RULE,	"Start Rule"),
		//PRM_Name("BuildingHeight",	"Building Height")
};

PRM_Default rpkDefault(0, "$HIP/$F.rpk");

PRM_Template NODE_PARAM_TEMPLATES[] = {
		//PRM_Template(PRM_STRING,   	1, &PRMgroupName,	0, &SOP_Node::pointGroupMenu),
		PRM_Template(PRM_FILE,		1, &NODE_PARAM_NAMES[0],		&rpkDefault, 0, 0, 0, &PRM_SpareData::fileChooserModeRead),
		PRM_Template(PRM_STRING,	1, &NODE_PARAM_NAMES[1],		PRMoneDefaults),
		PRM_Template(PRM_STRING,	1, &NODE_PARAM_NAMES[2],		PRMoneDefaults),
		PRM_Template(PRM_STRING,	1, &NODE_PARAM_NAMES[3],		PRMoneDefaults),
		//PRM_Template(PRM_FLT_J,		1, &NODE_PARAM_NAMES[4],		PRMoneDefaults),
		PRM_Template(),
};

} // namespace anonymous


// TODO: add support for multiple nodes
void newSopOperator(OP_OperatorTable *table) {
	UT_Exit::addExitCallback(dsoExit);

	prtConsoleLog = prt::ConsoleLogHandler::create(prt::LogHandler::ALL, prt::LogHandler::ALL_COUNT);
	prt::addLogHandler(prtConsoleLog);

	boost::filesystem::path sopPath;
	prt4hdn::utils::getPathToCurrentModule(sopPath);

	prt::FlexLicParams flp;
	std::string libflexnet = prt4hdn::utils::getSharedLibraryPrefix() + FILE_FLEXNET_LIB + prt4hdn::utils::getSharedLibrarySuffix();
	std::string libflexnetPath = (sopPath.parent_path() / libflexnet).string();
	flp.mActLibPath = libflexnetPath.c_str();
	flp.mFeature = "CityEngAdvFx";
	flp.mHostName = "";

	std::wstring libPath = (sopPath.parent_path() / PRT_LIB_SUBDIR).wstring();
	const wchar_t* extPaths[] = { libPath.c_str() };

	prt::Status status = prt::STATUS_UNSPECIFIED_ERROR;
	prtLicHandle = prt::init(extPaths, 1, prt::LOG_DEBUG, &flp, &status); // TODO: add UI for log level control

	if (prtLicHandle == 0 || status != prt::STATUS_OK)
		return;

	table->addOperator(new OP_Operator("prt4houdini",
			"prt4houdini",
			prt4hdn::SOP_PRT::myConstructor,
			NODE_PARAM_TEMPLATES,
			1,
			1,
			0)
	);
}


namespace prt4hdn {

OP_Node* SOP_PRT::myConstructor(OP_Network *net, const char *name, OP_Operator *op) {
	OP_Node* n = new SOP_PRT(net, name, op);
	return n;
}

SOP_PRT::SOP_PRT(OP_Network *net, const char *name, OP_Operator *op)
: SOP_Node(net, name, op)
, mPRTCache(prt::CacheObject::create(prt::CacheObject::CACHE_TYPE_DEFAULT))
, mAssetsMap(nullptr)
, mAttributeSource(prt::AttributeMapBuilder::create())
{
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

	mAllEncoders = boost::assign::list_of(ENCODER_ID_HOUDINI)(ENCODER_ID_CGA_ERROR)(ENCODER_ID_CGA_PRINT);
	mAllEncoderOptions = boost::assign::list_of(mHoudiniEncoderOptions)(mCGAErrorOptions)(mCGAPrintOptions);
}

SOP_PRT::~SOP_PRT() {
	mAttributeSource->destroy();

	if (mAssetsMap != nullptr)
		mAssetsMap->destroy();

	mHoudiniEncoderOptions->destroy();
	mCGAErrorOptions->destroy();
	mCGAPrintOptions->destroy();

	mPRTCache->destroy();
}

void SOP_PRT::createInitialShape(const GA_Group* group, void* ctx) {
	static const bool DBG = true;

	InitialShapeContext* isc = static_cast<InitialShapeContext*>(ctx);

	// preset attribute builder with node values
	// TODO for dynamic UI
	fpreal t = isc->mContext.getTime();
	//	size_t ai = setAttributes.find_first();
	//	while (ai < isc->mPrimitiveAttributes.size() && ai != boost::dynamic_bitset<>::npos) {
	//		const GA_ROAttributeRef& ar = isc->mPrimitiveAttributes[ai];
	//	double v = evalFloat("BuildingHeight", 0, t);
	//	//std::wstring wn = utils::toUTF16FromOSNarrow(ar->getName());
	//	isc->mAMB->setFloat(L"BuildingHeight", v);
	//	if (DBG) LOG_DBG << "   preset attr builder with node values " << "BuildingHeight" << " = " << v;
	//		setAttributes.reset(ai);
	//		ai = setAttributes.find_next(ai);
	//	}

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

	boost::dynamic_bitset<> setAttributes(isc->mPrimitiveAttributes.size(), 1);
	GA_Primitive* prim = nullptr;
	GA_FOR_ALL_GROUP_PRIMITIVES(gdp, static_cast<const GA_PrimitiveGroup*>(group), prim) {
		GU_PrimPoly* face = static_cast<GU_PrimPoly*>(prim);
		for(GA_Size i = face->getVertexCount()-1; i >= 0 ; i--) {
			GA_Offset off = face->getPointOffset(i);
			idx.push_back(static_cast<uint32_t>(off));
		}
		faceCounts.push_back(static_cast<uint32_t>(face->getVertexCount()));

		// extract attr
		size_t ai = setAttributes.find_first();
		while (ai < isc->mPrimitiveAttributes.size() && ai != boost::dynamic_bitset<>::npos) {
			if (DBG) LOG_DBG << "next unset attribute: " << ai;
			const GA_ROAttributeRef& ar = isc->mPrimitiveAttributes[ai];
			if (ar.isFloat() || ar.isInt()) {
				double v = prim->getValue<double>(ar);
				if (DBG) LOG_DBG << "   attrib " << ar->getName() << " = " << v;
				std::wstring wn = utils::toUTF16FromOSNarrow(ar->getName());
				mAttributeSource->setFloat(wn.c_str(), v);
			} else if (ar.isString()) {
				// TODO
			}
			setAttributes.reset(ai);
			ai = setAttributes.find_next(ai);
		}
	}

	if (DBG) LOG_DBG << "initial shape geo from group " << group->getName() << ": vtx = " << vtx << ", idx = " << idx << ", faceCounts = " << faceCounts;

	const prt::AttributeMap* initialShapeAttrs = mAttributeSource->createAttributeMap();

	isc->mISB->setGeometry(&vtx[0], vtx.size(), &idx[0], idx.size(), &faceCounts[0], faceCounts.size());

	std::wstring shapeName = utils::toUTF16FromOSNarrow(group->getName().toStdString());
	int32_t seed = 666; // TODO
	std::wstring startRule = mStyle + L"$" + mStartRule;
	isc->mISB->setAttributes(
			mRuleFile.c_str(),
			startRule.c_str(),
			seed,
			shapeName.c_str(),
			initialShapeAttrs,
			mAssetsMap
	);

	prt::Status status = prt::STATUS_UNSPECIFIED_ERROR;
	const prt::InitialShape* initialShape = isc->mISB->createInitialShapeAndReset(&status);
	if (status != prt::STATUS_OK) {
		LOG_WRN << "ignored input group '" << group->getName() << "': " << prt::getStatusDescription(status);
		initialShapeAttrs->destroy();
		return;
	}

	if (DBG) LOG_DBG << prt4hdn::utils::objectToXML(initialShape);

	isc->mInitialShapes.push_back(initialShape);
	isc->mInitialShapeAttributes.push_back(initialShapeAttrs);
}

OP_ERROR SOP_PRT::cookMySop(OP_Context &context) {
	if (!handleParams(context))
		return error();

	if (lockInputs(context) >= UT_ERROR_ABORT) {
		LOG_DBG << "lockInputs error";
		return error();
	}

	duplicateSource(0, context);

	if (error() < UT_ERROR_ABORT && cookInputGroups(context) < UT_ERROR_ABORT) {
		UT_AutoInterrupt progress("Generating PRT Geometry...");

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
	return error();
}

void getParamDef(const prt::RuleFileInfo* info, std::vector<std::string>& createdParams, std::ostream& defStream) {
	for(size_t i = 0; i < info->getNumAttributes(); i++) {
		if(info->getAttribute(i)->getNumParameters() != 0) continue;
		const wchar_t* attrName = info->getAttribute(i)->getName();
		std::string nAttrName = utils::toOSNarrowFromUTF16(attrName);
		nAttrName = nAttrName.substr(8); // TODO: better way to remove style for now...
		LOG_DBG << "rule attr: " << nAttrName;

//		parmDef << "parm {\n";
//		parmDef << "name    \"FloorHeight\"\n";
//		parmDef << "label   \"\"\n";
//		parmDef << "type    float\n";
//		//parmDef << "invisible\n";
//		parmDef << "size    1\n";
//		parmDef << "default { 5 }\n";
//		parmDef << "range   { 0 10 }\n";
//		parmDef << "export  none\n";
//		parmDef << "}\n";


		defStream << "parm {" << "\n";
		defStream << "    name  \"" << nAttrName << "\"" << "\n";
		defStream << "    label \"\"\n";


		switch(info->getAttribute(i)->getReturnType()) {
		case prt::AAT_BOOL: {
			//				for(size_t a = 0; a < info->getAttribute(i)->getNumAnnotations(); a++) {
			//					const prt::Annotation* an = info->getAttribute(i)->getAnnotation(a);
			//					if(!(wcscmp(an->getName(), ANNOT_RANGE)))
			//						e = new PRTEnum(prtNode, an);
			//				}
			//

			defStream << "    type    integer\n";
			break;
		}
		case prt::AAT_FLOAT: {
			//			double min = std::numeric_limits<double>::quiet_NaN();
			//			double max = std::numeric_limits<double>::quiet_NaN();
			//			for(size_t a = 0; a < info->getAttribute(i)->getNumAnnotations(); a++) {
			//				const prt::Annotation* an = info->getAttribute(i)->getAnnotation(a);
			//				if(!(wcscmp(an->getName(), ANNOT_RANGE))) {
			//					if(an->getNumArguments() == 2 && an->getArgument(0)->getType() == prt::AAT_FLOAT && an->getArgument(1)->getType() == prt::AAT_FLOAT) {
			//						min = an->getArgument(0)->getFloat();
			//						max = an->getArgument(1)->getFloat();
			//					} else
			//						e = new PRTEnum(prtNode, an);
			//				}
			//			}

			createdParams.push_back(nAttrName);


			defStream << "    type    float\n";
			break;
		}
		case prt::AAT_STR: {
			//			MString exts;
			//			bool    asFile  = false;
			//			bool    asColor = false;
			//			for(size_t a = 0; a < info->getAttribute(i)->getNumAnnotations(); a++) {
			//				const prt::Annotation* an = info->getAttribute(i)->getAnnotation(a);
			//				if(!(wcscmp(an->getName(), ANNOT_RANGE)))
			//					e = new PRTEnum(prtNode, an);
			//				else if(!(wcscmp(an->getName(), ANNOT_COLOR)))
			//					asColor = true;
			//				else if(!(wcscmp(an->getName(), ANNOT_DIR))) {
			//					exts = MString(an->getName());
			//					asFile = true;
			//				} else if(!(wcscmp(an->getName(), ANNOT_FILE))) {
			//					asFile = true;
			//					for(size_t arg = 0; arg < an->getNumArguments(); arg++) {
			//						if(an->getArgument(arg)->getType() == prt::AAT_STR) {
			//							exts += MString(an->getArgument(arg)->getStr());
			//							exts += " (*.";
			//							exts += MString(an->getArgument(arg)->getStr());
			//							exts += ");;";
			//						}
			//					}
			//					exts += "All Files (*.*)";
			//				}
			//			}
			//
			//			std::wstring value = evalAttrs.find(name.asWChar())->second.mString;
			//			MString mvalue(value.c_str());
			//			if(!(asColor) && mvalue.length() == 7 && value[0] == L'#')
			//				asColor = true;
			//
			//			if(e) {
			//				MCHECK(addEnumParameter(node, attr, name, mvalue, e));
			//			} else if(asFile) {
			//				MCHECK(addFileParameter(node, attr, name, mvalue, exts));
			//			} else if(asColor) {
			//				MCHECK(addColorParameter(node, attr, name, mvalue));
			//			} else {
			//				MCHECK(addStrParameter(node, attr, name, mvalue));
			//			}

			defStream << "    type    string\n";
			break;
		}
		default:
			break;
		}

		defStream << "    size    1\n";
//		parmDef << "default { 5 }\n";
//		parmDef << "range   { 0 10 }\n";
		defStream << "    export  none\n";
		defStream << "}\n";
	}
}

bool SOP_PRT::handleParams(OP_Context &context) {
	LOG_DBG << "handleParams()";
	fpreal now = context.getTime();

	// -- rule file
	// TODO: search/list cgbs
	// mRuleFile = L"bin/Candler Building.cgb";
	UT_String utRuleFile;
	evalString(utRuleFile, NODE_PARAM_RULE_FILE, 0, now);
	mRuleFile = utils::toUTF16FromOSNarrow(utRuleFile.toStdString());
	LOG_DBG << L"got rule file: " << mRuleFile;
	if (mRuleFile.empty()) {
		LOG_ERR << "rule file is empty/invalid, cannot continue";
		return false;
	}

	// -- rule package
	UT_String utNextRPKStr;
	evalString(utNextRPKStr, NODE_PARAM_RPK, 0, now);
	boost::filesystem::path nextRPKPath(utNextRPKStr.toStdString());
	if (boost::filesystem::exists(nextRPKPath)) {
		std::wstring nextRPKURI = L"file:/" + nextRPKPath.wstring();
		if (nextRPKURI != mRPKURI) {
			LOG_DBG << L"detected new RPK path: " << nextRPKURI;

			// clean up previous rpk context
			if (mAssetsMap != nullptr) {
				mAssetsMap->destroy();
				mAssetsMap = nullptr;
			}
			mPRTCache->flushAll();

			// rebuild assets map
			prt::Status status = prt::STATUS_UNSPECIFIED_ERROR;
			mAssetsMap = prt::createResolveMap(nextRPKURI.c_str(), 0, &status);
			if(status != prt::STATUS_OK) {
				LOG_ERR << "failed to create resolve map from '" << nextRPKURI << "', aborting.";
				return false;
			}

			// rebuild attribute UI
			status = prt::STATUS_UNSPECIFIED_ERROR;
			LOG_DBG << L"going to get cgbURI: mAssetsMap = " << mAssetsMap << L", mRuleFile = " << mRuleFile;
			if (!mAssetsMap->hasKey(mRuleFile.c_str()))
				return false;

			const wchar_t* cgbURI = mAssetsMap->getString(mRuleFile.c_str());
			LOG_DBG << L"got cgbURI = " << cgbURI;
			if (cgbURI == nullptr) {
				LOG_ERR << L"failed to resolve rule file '" << mRuleFile << "', aborting.";
				return false;
			}
			LOG_DBG << "going to createRuleFileInfo";
			const prt::RuleFileInfo* info = prt::createRuleFileInfo(cgbURI, 0, &status);
			if (status == prt::STATUS_OK) {

				mActiveParams.clear();;
				std::ostringstream paramDef;
				getParamDef(info, mActiveParams, paramDef);
				std::string s = paramDef.str();

				OP_Director* director = OPgetDirector();
				director->deleteAllNodeSpareParms(this);

				UT_IStream istr(&s[0], s.size(), UT_ISTREAM_ASCII);
				UT_String errors;
				director->loadNodeSpareParms(this, istr, errors);

				std::ostringstream ostr;
				director->saveNodeSpareParms(this, true, ostr);
				LOG_DBG << ostr.str();
				info->destroy();
			}

			// new rpk is "go"
			mRPKURI = nextRPKURI;
		}
	}

	// -- start rule
	// TODO: search for @StartRule
	// Default$Footprint
	UT_String utStyle;
	evalString(utStyle, NODE_PARAM_STYLE, 0, now);
	mStyle = utils::toUTF16FromOSNarrow(utStyle.toStdString());

	// -- start rule
	// TODO: search for @StartRule
	// Default$Footprint
	UT_String utStartRule;
	evalString(utStartRule, NODE_PARAM_START_RULE, 0, now);
	mStartRule = utils::toUTF16FromOSNarrow(utStartRule.toStdString());

	LOG_DBG << L"'style = " << mStyle << L", start rule = " << mStartRule;

	return true;
}

bool SOP_PRT::updateParmsFlags() {
	for (std::string& p: mActiveParams) {
		double v = evalFloat(p.c_str(), 0, 0); // TODO: time
		std::wstring wp = utils::toUTF16FromOSNarrow(p);
		mAttributeSource->setFloat(wp.c_str(), v);
	}

	forceRecook();

	bool changed = SOP_Node::updateParmsFlags();
	return changed;
}

} // namespace prt4hdn
