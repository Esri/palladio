#include "SOP_prt4houdini.h"

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

#include <vector>


namespace {


// some file name definitions
const char*		FILE_FLEXNET_LIB		= "flexnet_prt";
const char* 	FILE_LOG				= "prt4cmd.log";
const wchar_t*	FILE_CGA_ERROR			= L"CGAErrors.txt";
const wchar_t*	FILE_CGA_PRINT			= L"CGAPrint.txt";


// some encoder IDs
const wchar_t*	ENCODER_ID_CGA_ERROR	= L"com.esri.prt.core.CGAErrorEncoder";
const wchar_t*	ENCODER_ID_CGA_PRINT	= L"com.esri.prt.core.CGAPrintEncoder";
const wchar_t*	ENCODER_ID_HOUDINI		= L"HoudiniEncoder";


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


struct Logger {
};

// log to std streams
const std::wstring LEVELS[] = { L"trace", L"debug", L"info", L"warning", L"error", L"fatal" };
template<prt::LogLevel L> struct StreamLogger : public Logger {
	StreamLogger(std::wostream& out = std::wcout) : Logger(), mOut(out) { mOut << prefix(); }
	virtual ~StreamLogger() { mOut << std::endl; }
	StreamLogger<L>& operator<<(std::wostream&(*x)(std::wostream&)) { mOut << x; return *this; }
	StreamLogger<L>& operator<<(const std::string& x) { std::copy(x.begin(), x.end(), std::ostream_iterator<char, wchar_t>(mOut)); return *this; }
	template<typename T> StreamLogger<L>& operator<<(const T& x) { mOut << x; return *this; }
	static std::wstring prefix() { return L"[" + LEVELS[L] + L"] "; }
	std::wostream& mOut;
};

// log through the prt logger
template<prt::LogLevel L> struct PRTLogger : public Logger {
	PRTLogger() : Logger() { }
	virtual ~PRTLogger() { prt::log(wstr.str().c_str(), L); }
	PRTLogger<L>& operator<<(std::wostream&(*x)(std::wostream&)) { wstr << x;  return *this; }
	PRTLogger<L>& operator<<(const std::string& x) {
		std::copy(x.begin(), x.end(), std::ostream_iterator<char, wchar_t>(wstr));
		return *this;
	}
	template<typename T> PRTLogger<L>& operator<<(const std::vector<T>& v) { 
		wstr << L"[ ";
		BOOST_FOREACH(const T& x, v) {
			wstr << x << L" ";
		}
		wstr << L"]";
		return *this;
	}
	template<typename T> PRTLogger<L>& operator<<(const T& x) { wstr << x; return *this; }
	std::wostringstream wstr;
};

// log to Houdini message
// template<prt::LogLevel L> struct HoudiniLogger : public Logger {
// 	HoudiniLogger() : Logger() { }
// 	virtual ~HoudiniLogger() { 
// 		switch (L) {
// 			case prt::LOG_DEBUG:
// 			case prt::LOG_INFO:
// 				
// 		prt::log(wstr.str().c_str(), L);
// 		
// 	}
// 	HoudiniLogger<L>& operator<<(std::wostream&(*x)(std::wostream&)) { wstr << x;  return *this; }
// 	HoudiniLogger<L>& operator<<(const std::string& x) {
// 		std::copy(x.begin(), x.end(), std::ostream_iterator<char, wchar_t>(wstr));
// 		return *this;
// 	}
// 	template<typename T> HoudiniLogger<L>& operator<<(const T& x) { wstr << x; return *this; }
// 	std::ostringstream str;
// };

#define LT PRTLogger

typedef LT<prt::LOG_DEBUG>		_LOG_DBG;
typedef LT<prt::LOG_INFO>		_LOG_INF;
typedef LT<prt::LOG_WARNING>	_LOG_WRN;
typedef LT<prt::LOG_ERROR>		_LOG_ERR;

#define LOG_DBG _LOG_DBG()
#define LOG_INF _LOG_INF()
#define LOG_WRN _LOG_WRN()
#define LOG_ERR _LOG_ERR()


const prt::AttributeMap* createValidatedOptions(const wchar_t* encID, const prt::AttributeMap* unvalidatedOptions) {
	const prt::EncoderInfo* encInfo = prt::createEncoderInfo(encID);
	const prt::AttributeMap* validatedOptions = 0;
	const prt::AttributeMap* optionStates = 0;
	encInfo->createValidatedOptionsAndStates(unvalidatedOptions, &validatedOptions, &optionStates);
	optionStates->destroy();
	encInfo->destroy();
	return validatedOptions;
}


std::string objectToXML(prt::Object const* obj) {
	if (obj == 0)
		throw std::invalid_argument("object pointer is not valid");
	const size_t siz = 4096;
	size_t actualSiz = siz;
	std::string buffer(siz, ' ');
	obj->toXML((char*)&buffer[0], &actualSiz);
	buffer.resize(actualSiz-1); // ignore terminating 0
	if(siz < actualSiz)
		obj->toXML((char*)&buffer[0], &actualSiz);
	return buffer;
}


} // namespace anonymous


class HoudiniGeometry : public HoudiniCallbacks {
	virtual void setVertices(double* vtx, size_t size) {
	}
	virtual void setNormals(double* nrm, size_t size) {
	}
	virtual void setUVs(float* u, float* v, size_t size) {
	}

	virtual void setFaces(int* counts, size_t countsSize, int* connects, size_t connectsSize, int* uvCounts, size_t uvCountsSize, int* uvConnects, size_t uvConnectsSize) {
	}
	virtual void createMesh() {
	}
	virtual void finishMesh() {
	}

	virtual void matSetColor(int start, int count, float r, float g, float b) {
	}
	virtual void matSetDiffuseTexture(int start, int count, const wchar_t* tex) {
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
};

using namespace HDK_Sample;
// namespace HDK_Sample {

static PRM_Name names[] = {
    PRM_Name("rpk",			"Rule Package"),
    PRM_Name("startRule",	"Start Rule"),
    PRM_Name("seed",		"Random Seed"),
    PRM_Name("name",		"Initial Shape Name")
};

static PRM_Default rpkDefault(0, "$HIP/$F.rpk");

PRM_Template SOP_PRT::myTemplateList[] = {
    PRM_Template(PRM_STRING,   	1, &PRMgroupName,	0, &SOP_Node::pointGroupMenu),
    PRM_Template(PRM_FILE,		1, &names[0],		&rpkDefault, 0, 0, 0, &PRM_SpareData::fileChooserModeRead),
    PRM_Template(PRM_STRING,	1, &names[1],		PRMoneDefaults),
    PRM_Template(PRM_FLT_J,		1, &names[2],		PRMzeroDefaults),
    PRM_Template(PRM_STRING,	1, &names[3],		PRMoneDefaults),
    PRM_Template(),
};


 //PRM_Template(PRM_STRING,    PRM_TYPE_DYNAMIC_PATH, 1, &copnames[4], 0, 0, 0, 0, &PRM_SpareData::cop2Path),
  //  PRM_Template(PRM_STRING,    1, &copnames[3], &colorDef, &colorMenu),
   // PRM_Template(PRM_FLT_J,     1, &copnames[1], &copFrameDefault),
//     PRM_Template(PRM_PICFILE,   1, &copnames[2], &fileDef,
//                                 0, 0, 0, &PRM_SpareData::fileChooserModeRead),

void newSopOperator(OP_OperatorTable *table) {
	UT_Exit::addExitCallback(dsoExit);

	prtConsoleLog = prt::ConsoleLogHandler::create(prt::LogHandler::ALL, prt::LogHandler::ALL_COUNT);
	prt::addLogHandler(prtConsoleLog);

	prt::FlexLicParams flp;
	flp.mActLibPath = "/home/shaegler/procedural/dev/builds/cesdk/esri_ce_sdk_1_2_1591_rhel6_gcc44_rel_opt/bin/libflexnet_prt.so";
	flp.mFeature = "CityEngAdvFx";

	const wchar_t* extPaths[] = {
		 L"/home/shaegler/procedural/dev/builds/cesdk/esri_ce_sdk_1_2_1591_rhel6_gcc44_rel_opt/lib",
		 L"/home/shaegler/git/esri-cityengine-sdk/examples/prt4houdini/codec/install/lib"
	};
	prt::Status status = prt::STATUS_UNSPECIFIED_ERROR;
	prtLicHandle = prt::init(extPaths, 2, prt::LOG_DEBUG, &flp, &status);
	
	if (prtLicHandle == 0 || status != prt::STATUS_OK)
		return;
	 
	prtCache = prt::CacheObject::create(prt::CacheObject::CACHE_TYPE_DEFAULT);
	
	table->addOperator(new OP_Operator("prt4houdini",
					"prt4houdini",
					 SOP_PRT::myConstructor,
					 SOP_PRT::myTemplateList,
					 1,
					 1,
					 0));
}

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

//     fpreal t = context.getTime();
//     float phase = PHASE(t);
//     float amp = AMP(t);
//     float period = PERIOD(t);

    if (error() < UT_ERROR_ABORT && cookInputGroups(context) < UT_ERROR_ABORT) {
		UT_AutoInterrupt progress("Generating PRT Geometry...");
		
		std::vector<double> vtx;
		std::vector<uint32_t> idx, faceCounts;
		
		GA_Offset ptoff;
		GA_FOR_ALL_GROUP_PTOFF(gdp, mPointGroup, ptoff) {
			UT_Vector3 p = gdp->getPos3(ptoff);
			vtx.push_back(static_cast<double>(p.x()));
			vtx.push_back(static_cast<double>(p.y()));
			vtx.push_back(static_cast<double>(p.z()));
		}

		GA_Primitive* prim = 0;
		GA_FOR_ALL_GROUP_PRIMITIVES(gdp, mPrimitiveGroup, prim) {
			for(GA_Size i = prim->getVertexCount()-1; i >=0 ; i--) { // houdini clockwise?
				GA_Offset off = prim->getVertexOffset(i);
				idx.push_back(static_cast<uint32_t>(off));
			}
			faceCounts.push_back(static_cast<uint32_t>(prim->getVertexCount()));
		}
		
		LOG_DBG << "initial shape geo: vtx = " << vtx << ", idx = " << idx << ", faceCounts = " << faceCounts;
		
		HoudiniGeometry hg;
		prt::CacheObject* cache = prt::CacheObject::create(prt::CacheObject::CACHE_TYPE_DEFAULT);
		{ // create scope to better see lifetime of callback and cache

			std::wstring rpkURI = L"file:/home/shaegler/procedural/dev/builds/cesdk/data/candler.02.rpk";

			prt::Status status = prt::STATUS_UNSPECIFIED_ERROR;
			const prt::ResolveMap* assetsMap = prt::createResolveMap(rpkURI.c_str(), 0, &status);
			if(status != prt::STATUS_OK) {
				LOG_ERR << "getting resolve map from '" << rpkURI << "' failed, aborting.";
				return error();
			}

			prt::AttributeMapBuilder* bld = prt::AttributeMapBuilder::create();
			const prt::AttributeMap* initialShapeAttrs = bld->createAttributeMapAndReset();
			bld->destroy();

			std::wstring shapeName = L"TheInitialShape";
			std::wstring ruleFile = L"bin/candler.01.cgb";
			std::wstring startRule = L"default$Lot";
			int32_t seed = 666;

			prt::InitialShapeBuilder* isb = prt::InitialShapeBuilder::create();
// 			isb->setGeometry(&vtx[0], vtx.size(), &idx[0], idx.size(), &faceCounts[0], faceCounts.size());
			isb->setGeometry(UnitQuad::vertices, UnitQuad::vertexCount, UnitQuad::indices, UnitQuad::indexCount, UnitQuad::faceCounts, UnitQuad::faceCountsCount);
			isb->setAttributes(
					ruleFile.c_str(),
					startRule.c_str(),
					seed,
					shapeName.c_str(),
					initialShapeAttrs,
					assetsMap
			);
			const prt::InitialShape* initialShape = isb->createInitialShapeAndReset();
			LOG_DBG << objectToXML(initialShape);
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
			const prt::AttributeMap* validatedEncOpts = createValidatedOptions(ENCODER_ID_HOUDINI, encoderOptions);
			const prt::AttributeMap* validatedErrOpts = createValidatedOptions(ENCODER_ID_CGA_ERROR, errOptions);
			const prt::AttributeMap* validatedPrintOpts = createValidatedOptions(ENCODER_ID_CGA_PRINT, printOptions);

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
    }

    unlockInputs();
    
    return error();
}

	// const char* SOP_PRT::inputLabel(unsigned) const {
	//     return "Initial Shape Geometry";
	// }

// } // namespace
