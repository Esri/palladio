#include "SOP_prt4houdini.h"
#include "logging.h"
#include "utils.h"

#include "HoudiniCallbacks.h"

#include "prt/API.h"
#include "prt/FlexLicParams.h"

#include "SYS/SYS_Math.h"
#include "UT/UT_DSOVersion.h"
#include "UT/UT_Matrix3.h"
#include "UT/UT_Matrix4.h"
#include "UT/UT_Exit.h"
#include "GU/GU_Detail.h"
#include "GU/GU_PrimPoly.h"
#include "PRM/PRM_Include.h"
#include "PRM/PRM_SpareData.h"
#include "OP/OP_Operator.h"
#include "OP/OP_OperatorTable.h"
#include "SOP/SOP_Guide.h"
#include "GOP/GOP_GroupParse.h"

#include "boost/foreach.hpp"
#include "boost/filesystem.hpp"
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

// prt objects with same life-cycle as SOP
const prt::Object* prtLicHandle = 0;
prt::CacheObject* prtCache = 0;
prt::ConsoleLogHandler*	prtConsoleLog = 0;

void dsoExit(void*) {
	if (prtCache)
		prtCache->destroy();
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

PRM_Name names[] = {
		PRM_Name("rpk",			"Rule Package"),
		PRM_Name("startRule",	"Start Rule"),
		PRM_Name("seed",		"Random Seed"),
		PRM_Name("name",		"Initial Shape Name"),
		PRM_Name("height",		"height")
};

PRM_Default rpkDefault(0, "$HIP/$F.rpk");

struct RulePackageContext {

};

typedef std::vector<const prt::InitialShape*> InitialShapePtrVector;
typedef std::vector<const prt::AttributeMap*> AttributeMapPtrVector;

struct InitialShapeContext {
	InitialShapeContext(OP_Context& ctx) : mContext(ctx) {
		// test rpk for development...
		boost::filesystem::path sopPath;
		prt4hdn::utils::getPathToCurrentModule(sopPath);
		std::wstring rpkURI = L"file:" + (sopPath.parent_path() / "prtdata" / "candler.rpk").wstring();

		prt::Status status = prt::STATUS_UNSPECIFIED_ERROR;
		mAssetsMap = prt::createResolveMap(rpkURI.c_str(), 0, &status);
		if(status != prt::STATUS_OK) {
			LOG_ERR << "getting resolve map from '" << rpkURI << "' failed, aborting.";
			abort(); // TODO
		}

		mAMB = prt::AttributeMapBuilder::create();
		mISB = prt::InitialShapeBuilder::create();
	}

	~InitialShapeContext() {
		BOOST_FOREACH(const prt::InitialShape* is, mInitialShapes) {
			is->destroy();
		}

		BOOST_FOREACH(const prt::AttributeMap* am, mInitialShapeAttributes) {
			am->destroy();
		}

		mISB->destroy();
		mAMB->destroy();
		mAssetsMap->destroy();
	}

	OP_Context& mContext;

	const prt::ResolveMap* mAssetsMap;
	prt::InitialShapeBuilder* mISB;
	InitialShapePtrVector mInitialShapes;
	prt::AttributeMapBuilder* mAMB;
	AttributeMapPtrVector mInitialShapeAttributes;
};

PRM_Template myTemplateList[] = {
		PRM_Template(PRM_STRING,   	1, &PRMgroupName,	0, &SOP_Node::pointGroupMenu),
		PRM_Template(PRM_FILE,		1, &names[0],		&rpkDefault, 0, 0, 0, &PRM_SpareData::fileChooserModeRead),
		PRM_Template(PRM_STRING,	1, &names[1],		PRMoneDefaults),
		PRM_Template(PRM_FLT_J,		1, &names[2],		PRMzeroDefaults),
		PRM_Template(PRM_STRING,	1, &names[3],		PRMoneDefaults),
		PRM_Template(PRM_FLT_J,		1, &names[4],		PRMoneDefaults),
		PRM_Template(),
};

} // namespace anonymous


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

	prtCache = prt::CacheObject::create(prt::CacheObject::CACHE_TYPE_DEFAULT);

	table->addOperator(new OP_Operator("prt4houdini",
			"prt4houdini",
			prt4hdn::SOP_PRT::myConstructor,
			myTemplateList,
			1,
			1,
			0)
	);
}


namespace prt4hdn {

OP_Node* SOP_PRT::myConstructor(OP_Network *net, const char *name, OP_Operator *op) {
	return new SOP_PRT(net, name, op);
}

SOP_PRT::SOP_PRT(OP_Network *net, const char *name, OP_Operator *op)
: SOP_Node(net, name, op)
, mPRTCache(prt::CacheObject::create(prt::CacheObject::CACHE_TYPE_DEFAULT))
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
	mHoudiniEncoderOptions->destroy();
	mCGAErrorOptions->destroy();
	mCGAPrintOptions->destroy();

	mPRTCache->destroy();
}

void SOP_PRT::createInitialShape(const GA_Group* group, void* ctx) {
	static const bool DBG = false;

	InitialShapeContext* isc = static_cast<InitialShapeContext*>(ctx);

	std::vector<double> vtx;
	std::vector<uint32_t> idx, faceCounts;

	GA_Offset ptoff;
	GA_FOR_ALL_PTOFF(gdp, ptoff) {
		UT_Vector3 p = gdp->getPos3(ptoff);
		vtx.push_back(static_cast<double>(p.x()));
		vtx.push_back(static_cast<double>(p.y()));
		vtx.push_back(static_cast<double>(p.z()));
	}

	GA_Primitive* prim = 0;
	GA_FOR_ALL_GROUP_PRIMITIVES(gdp, static_cast<const GA_PrimitiveGroup*>(group), prim) {
		GU_PrimPoly* face = static_cast<GU_PrimPoly*>(prim);
		for(GA_Size i = face->getVertexCount()-1; i >= 0 ; i--) {
			GA_Offset off = face->getPointOffset(i);
			idx.push_back(static_cast<uint32_t>(off));
		}
		faceCounts.push_back(static_cast<uint32_t>(face->getVertexCount()));
	}

	if (DBG) LOG_DBG << "initial shape geo from group " << group->getName() << ": vtx = " << vtx << ", idx = " << idx << ", faceCounts = " << faceCounts;


	// TODO: optimize creating the initialshape object (how to detect when input geo changes?)

	fpreal t = isc->mContext.getTime();
	float height = evalFloat("height", 0, t);
	isc->mAMB->setFloat(L"BuildingHeight", height);
	const prt::AttributeMap* initialShapeAttrs = isc->mAMB->createAttributeMapAndReset();

	std::wstring shapeName = utils::toUTF16FromOSNarrow(group->getName().toStdString());
	std::wstring ruleFile = L"bin/Candler Building.cgb";
	std::wstring startRule = L"Default$Footprint";
	int32_t seed = 666;

	isc->mISB->setGeometry(&vtx[0], vtx.size(), &idx[0], idx.size(), &faceCounts[0], faceCounts.size());
	isc->mISB->setAttributes(
			ruleFile.c_str(),
			startRule.c_str(),
			seed,
			shapeName.c_str(),
			initialShapeAttrs,
			isc->mAssetsMap
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
	if (lockInputs(context) >= UT_ERROR_ABORT)
		return error();

	duplicateSource(0, context);

	if (error() < UT_ERROR_ABORT && cookInputGroups(context) < UT_ERROR_ABORT) {
		UT_AutoInterrupt progress("Generating PRT Geometry...");

		InitialShapeContext isc(context);
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

} // namespace prt4hdn
