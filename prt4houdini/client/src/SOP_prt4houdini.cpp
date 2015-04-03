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

#include "boost/foreach.hpp"
#include "boost/filesystem.hpp"

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
		prtLicHandle->destroy();
	if (prtConsoleLog)
		prt::removeLogHandler(prtConsoleLog);
}

class HoudiniGeometry : public HoudiniCallbacks {
public:
	HoudiniGeometry(GU_Detail* gdp) : mDetail(gdp) { }

protected:
	virtual void setVertices(double* vtx, size_t size) {
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
		mIndices.insert(mIndices.end(), connects, connects+connectsSize);
	}

	virtual void matSetColor(int start, int count, float r, float g, float b) {
		// TODO
	}

	virtual void matSetDiffuseTexture(int start, int count, const wchar_t* tex) {
		// TODO
	}

	virtual void createMesh() {
		// TODO
	}

	virtual void finishMesh() {
		GU_PrimPoly::buildBlock(mDetail, &mPoints[0], mPoints.size(), mPolyCounts, &mIndices[0]);
		mPolyCounts.clear();
		mIndices.clear();
		mPoints.clear();
		LOG_DBG << "finishMesh";
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
	prtLicHandle = prt::init(extPaths, 1, prt::LOG_DEBUG, &flp, &status);

	if (prtLicHandle == 0 || status != prt::STATUS_OK)
		return;

	prtCache = prt::CacheObject::create(prt::CacheObject::CACHE_TYPE_DEFAULT);

	table->addOperator(new OP_Operator("prt4houdini",
			"prt4houdini",
			prt4hdn::SOP_PRT::myConstructor,
			prt4hdn::SOP_PRT::myTemplateList,
			1,
			1,
			0)
	);
}


namespace prt4hdn {

PRM_Template SOP_PRT::myTemplateList[] = {
		PRM_Template(PRM_STRING,   	1, &PRMgroupName,	0, &SOP_Node::pointGroupMenu),
		PRM_Template(PRM_FILE,		1, &names[0],		&rpkDefault, 0, 0, 0, &PRM_SpareData::fileChooserModeRead),
		PRM_Template(PRM_STRING,	1, &names[1],		PRMoneDefaults),
		PRM_Template(PRM_FLT_J,		1, &names[2],		PRMzeroDefaults),
		PRM_Template(PRM_STRING,	1, &names[3],		PRMoneDefaults),
		PRM_Template(PRM_FLT_J,		1, &names[4],		PRMoneDefaults),
		PRM_Template(),
};

//PRM_Template(PRM_STRING,    PRM_TYPE_DYNAMIC_PATH, 1, &copnames[4], 0, 0, 0, 0, &PRM_SpareData::cop2Path),
//  PRM_Template(PRM_STRING,    1, &copnames[3], &colorDef, &colorMenu),
// PRM_Template(PRM_FLT_J,     1, &copnames[1], &copFrameDefault),
//     PRM_Template(PRM_PICFILE,   1, &copnames[2], &fileDef,
//                                 0, 0, 0, &PRM_SpareData::fileChooserModeRead),

OP_Node* SOP_PRT::myConstructor(OP_Network *net, const char *name, OP_Operator *op) {
	return new SOP_PRT(net, name, op);
}

SOP_PRT::SOP_PRT(OP_Network *net, const char *name, OP_Operator *op)
: SOP_Node(net, name, op)
, mPointGroup(0)
, mPrimitiveGroup(0) {
}

SOP_PRT::~SOP_PRT() {
}

OP_ERROR SOP_PRT::cookInputGroups(OP_Context &context, int alone) {
	cookInputPointGroups(context, mPointGroup, myDetailGroupPair);
	return cookInputPrimitiveGroups(context, mPrimitiveGroup, myDetailGroupPair);
}

// debug geo
namespace UnitQuad {
const double 	vertices[]				= { 0, 0, 0,  0, 0, 1,  1, 0, 1,  1, 0, 0 };
const size_t 	vertexCount				= 12;
const uint32_t	indices[]				= { 0, 1, 2, 3 };
const size_t 	indexCount				= 4;
const uint32_t 	faceCounts[]			= { 4 };
const size_t 	faceCountsCount			= 1;
}

OP_ERROR SOP_PRT::cookMySop(OP_Context &context) {
	if (lockInputs(context) >= UT_ERROR_ABORT)
		return error();

	duplicateSource(0, context);

	if (error() < UT_ERROR_ABORT && cookInputGroups(context) < UT_ERROR_ABORT) {
		UT_AutoInterrupt progress("Generating PRT Geometry...");

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
		GA_FOR_ALL_PRIMITIVES(gdp, prim) {
			GU_PrimPoly* face = (GU_PrimPoly*)prim;
			for(GA_Size i = face->getVertexCount()-1; i >= 0 ; i--) {
				GA_Offset off = face->getPointOffset(i);
				idx.push_back(static_cast<uint32_t>(off));
			}
			faceCounts.push_back(static_cast<uint32_t>(face->getVertexCount()));
		}

		LOG_DBG << "initial shape geo: vtx = " << vtx << ", idx = " << idx << ", faceCounts = " << faceCounts;

		gdp->clearAndDestroy();

		HoudiniGeometry hg(gdp);
		prt::CacheObject* cache = prt::CacheObject::create(prt::CacheObject::CACHE_TYPE_DEFAULT);
		{ // create scope to better see lifetime of callback and cache

			// test rpk for development...
			boost::filesystem::path sopPath;
			utils::getPathToCurrentModule(sopPath);
			std::wstring rpkURI = L"file:" + (sopPath.parent_path() / "prtdata" / "candler.rpk").wstring();

			prt::Status status = prt::STATUS_UNSPECIFIED_ERROR;
			const prt::ResolveMap* assetsMap = prt::createResolveMap(rpkURI.c_str(), 0, &status);
			if(status != prt::STATUS_OK) {
				LOG_ERR << "getting resolve map from '" << rpkURI << "' failed, aborting.";
				return error();
			}

			fpreal t = context.getTime();
			float height = evalFloat("height", 0, t);
			prt::AttributeMapBuilder* bld = prt::AttributeMapBuilder::create();
			bld->setFloat(L"BuildingHeight", height);
			const prt::AttributeMap* initialShapeAttrs = bld->createAttributeMapAndReset();
			bld->destroy();

			std::wstring shapeName = L"TheInitialShape";
			std::wstring ruleFile = L"bin/Candler Building.cgb";
			std::wstring startRule = L"Default$Footprint";
			int32_t seed = 666;

			prt::InitialShapeBuilder* isb = prt::InitialShapeBuilder::create();
			isb->setGeometry(&vtx[0], vtx.size(), &idx[0], idx.size(), &faceCounts[0], faceCounts.size());
			//isb->setGeometry(UnitQuad::vertices, UnitQuad::vertexCount, UnitQuad::indices, UnitQuad::indexCount, UnitQuad::faceCounts, UnitQuad::faceCountsCount);
			isb->setAttributes(
					ruleFile.c_str(),
					startRule.c_str(),
					seed,
					shapeName.c_str(),
					initialShapeAttrs,
					assetsMap
			);
			const prt::InitialShape* initialShape = isb->createInitialShapeAndReset();
			LOG_DBG << utils::objectToXML(initialShape);
			isb->destroy();

			// -- setup options for helper encoders
			prt::AttributeMapBuilder* optionsBuilder = prt::AttributeMapBuilder::create();

			const prt::AttributeMap* encoderOptions = optionsBuilder->createAttributeMapAndReset();

			optionsBuilder->setString(L"name", FILE_CGA_ERROR);
			const prt::AttributeMap* errOptions = optionsBuilder->createAttributeMapAndReset();

			optionsBuilder->setString(L"name", FILE_CGA_PRINT);
			const prt::AttributeMap* printOptions = optionsBuilder->createAttributeMapAndReset();

			optionsBuilder->destroy();

			// -- validate & complete encoder options
			const prt::AttributeMap* validatedEncOpts = utils::createValidatedOptions(ENCODER_ID_HOUDINI, encoderOptions);
			const prt::AttributeMap* validatedErrOpts = utils::createValidatedOptions(ENCODER_ID_CGA_ERROR, errOptions);
			const prt::AttributeMap* validatedPrintOpts = utils::createValidatedOptions(ENCODER_ID_CGA_PRINT, printOptions);

			// -- THE GENERATE CALL
			const prt::AttributeMap* encoderOpts[] = { validatedEncOpts, validatedErrOpts, validatedPrintOpts };
			const wchar_t* encoders[] = {
					ENCODER_ID_HOUDINI,		// the houdini geometry encoder
					ENCODER_ID_CGA_ERROR,	// an encoder to redirect rule errors into CGAErrors.txt
					ENCODER_ID_CGA_PRINT	// an encoder to redirect CGA print statements to CGAPrint.txt
			};
			prt::Status stat = prt::generate(&initialShape, 1, 0, encoders, 3, encoderOpts, &hg, cache, 0);
			if(stat != prt::STATUS_OK) {
				LOG_ERR << "prt::generate() failed with status: '" << prt::getStatusDescription(stat) << "' (" << stat << ")";
			}

			// -- cleanup
			encoderOptions->destroy();
			errOptions->destroy();
			printOptions->destroy();
			validatedEncOpts->destroy();
			validatedErrOpts->destroy();
			validatedPrintOpts->destroy();
			initialShape->destroy();
			initialShapeAttrs->destroy();
			assetsMap->destroy();
		}
		cache->destroy();

		select(GU_SPrimitive);
	}

	unlockInputs();

	return error();
}

// const char* SOP_PRT::inputLabel(unsigned) const {
//     return "Initial Shape Geometry";
// }

} // namespace prt4hdn
