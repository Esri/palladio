#include "../client/PRTContext.h"
#include "../client/utils.h"
#include "../client/ModelConverter.h"
#include "../codec/encoder/HoudiniEncoder.h"

#include "prt/AttributeMap.h"
#include "prtx/Geometry.h"

#include "boost/filesystem/path.hpp"

#define CATCH_CONFIG_RUNNER
#include "catch.hpp"

#include <algorithm>


namespace {

PRTContextUPtr prtCtx;

} // namespace


int main( int argc, char* argv[] ) {
	assert(!prtCtx);

	const std::vector<boost::filesystem::path> addExtDirs = {
		"../lib", // adapt to default prt dir layout (core is in bin subdir)
		HOUDINI_CODEC_PATH // set to absolute path to houdini encoder lib via cmake
	};

	prtCtx.reset(new PRTContext(addExtDirs));
	int result = Catch::Session().run( argc, argv );
	prtCtx.reset();
	return result;
}


// -- client test cases

TEST_CASE("create file URI from path", "[utils]" ) {
    CHECK(toFileURI(boost::filesystem::path("/tmp/foo.txt")) == L"file:/tmp/foo.txt");
}

TEST_CASE("percent-encode a UTF-8 string", "[utils]") {
    CHECK(percentEncode("with space") == L"with%20space");
}

TEST_CASE("get XML representation of a PRT object", "[utils]") {
    AttributeMapBuilderUPtr amb(prt::AttributeMapBuilder::create());
	amb->setString(L"foo", L"bar");
    AttributeMapUPtr am(amb->createAttributeMap());
    std::string xml = objectToXML(am.get());
    CHECK(xml == "<attributable>\n\t<attribute key=\"foo\" value=\"bar\" type=\"str\"/>\n</attributable>");
}

TEST_CASE("deserialize uv set") {
	const prtx::IndexVector faceCnt  = { 4 };
	const prtx::IndexVector uvCounts = { 4, 4 };
	const prtx::IndexVector uvIdx    = { 3, 2, 1, 0,  4, 5, 6, 7 };
	std::vector<uint32_t> uvIndicesPerSet;

	SECTION("test uv set 0") {
		ModelConversion::getUVSet(uvIndicesPerSet, faceCnt.data(), faceCnt.size(), uvCounts.data(), uvCounts.size(), 0, 2, uvIdx.data(), uvIdx.size());
		const prtx::IndexVector uvIndicesPerSetExp = { 3, 2, 1, 0 };
		CHECK(uvIndicesPerSet == uvIndicesPerSetExp);
	}

	SECTION("test uv set 1") {
		ModelConversion::getUVSet(uvIndicesPerSet, faceCnt.data(), faceCnt.size(), uvCounts.data(), uvCounts.size(), 1, 2, uvIdx.data(), uvIdx.size());
		const prtx::IndexVector uvIndicesPerSetExp = { 4, 5, 6, 7 };
		CHECK(uvIndicesPerSet == uvIndicesPerSetExp);
	}
}


// -- encoder test cases

TEST_CASE("serialize basic mesh") {
	const prtx::IndexVector  faceCnt   = { 4 };
	const prtx::DoubleVector vtx       = { 0.0, 0.0, 0.0,  1.0, 0.0, 0.0,  1.0, 0.0, 1.0,  0.0, 0.0, 1.0 };
	const prtx::IndexVector  vtxInd    = { 0, 1, 2, 3 };
	const prtx::IndexVector  vtxIndRev = [&vtxInd]() { auto c = vtxInd; std::reverse_copy(vtxInd.begin(), vtxInd.end(), c.begin()); return c; }();

    prtx::MeshBuilder mb;
    mb.addVertexCoords(vtx);
    uint32_t faceIdx = mb.addFace();
    mb.setFaceVertexIndices(faceIdx, vtxInd);

    prtx::GeometryBuilder gb;
    gb.addMesh(mb.createShared());

    auto geo = gb.createShared();
	CHECK(geo->getMeshes().front()->getUVSetsCount() == 0);

    const prtx::GeometryPtrVector geos = { geo };

    SerializedGeometry sg;
    serializeGeometry(sg, geos);

    CHECK(sg.counts == faceCnt);
	CHECK(sg.coords == vtx);
	CHECK(sg.indices == vtxIndRev); // reverses winding

	CHECK(sg.uvSets == 0);

	const prtx::IndexVector uvCountsExp = { };
	CHECK(sg.uvCounts == uvCountsExp);
}

TEST_CASE("serialize mesh with one uv set") {
	const prtx::IndexVector  faceCnt   = { 4 };
	const prtx::DoubleVector vtx       = { 0.0, 0.0, 0.0,  1.0, 0.0, 0.0,  1.0, 0.0, 1.0,  0.0, 0.0, 1.0 };
	const prtx::IndexVector  vtxIdx    = { 0, 1, 2, 3 };
	const prtx::DoubleVector uvs       = { 0.0, 0.0,  1.0, 0.0,  1.0, 1.0,  0.0, 1.0 };
	const prtx::IndexVector  uvIdx     = { 0, 1, 2, 3 };

    prtx::MeshBuilder mb;
    mb.addVertexCoords(vtx);
	mb.addUVCoords(0, uvs);
    uint32_t faceIdx = mb.addFace();
    mb.setFaceVertexIndices(faceIdx, vtxIdx);
	mb.setFaceUVIndices(faceIdx, 0, uvIdx);
	const auto m = mb.createShared();

	CHECK(m->getUVSetsCount() == 2); // TODO: correct would be 1!!!

    prtx::GeometryBuilder gb;
    gb.addMesh(m);
    auto geo = gb.createShared();
    const prtx::GeometryPtrVector geos = { geo };

    SerializedGeometry sg;
    serializeGeometry(sg, geos);

    CHECK(sg.counts == faceCnt);
    CHECK(sg.coords == vtx);
	const prtx::IndexVector vtxIdxExp = { 3, 2, 1, 0 };
	CHECK(sg.indices == vtxIdxExp);

	CHECK(sg.uvCoords == uvs);

	CHECK(sg.uvSets == 2); // TODO: actually wrong expected value

	const prtx::IndexVector uvCountsExp = { 4, 0 };
	CHECK(sg.uvCounts == uvCountsExp);

	const prtx::IndexVector uvIdxExp = { 3, 2, 1, 0 };
	CHECK(sg.uvIndices == uvIdxExp);
}

TEST_CASE("serialize mesh with two uv sets") {
	const prtx::IndexVector  faceCnt = { 4 };
	const prtx::DoubleVector vtx     = { 0.0, 0.0, 0.0,  1.0, 0.0, 0.0,  1.0, 0.0, 1.0,  0.0, 0.0, 1.0 };
	const prtx::IndexVector  vtxIdx  = { 0, 1, 2, 3 };
	const prtx::DoubleVector uvs0    = { 0.0, 0.0,  1.0, 0.0,  1.0, 1.0,  0.0, 1.0 };
	const prtx::IndexVector  uvIdx0  = { 0, 1, 2, 3 };
	const prtx::DoubleVector uvs1    = { 0.0, 0.0,  0.5, 0.0,  0.5, 0.5,  0.0, 0.5 };
	const prtx::IndexVector  uvIdx1  = { 3, 2, 1, 0 };

    prtx::MeshBuilder mb;
    mb.addVertexCoords(vtx);
	mb.addUVCoords(0, uvs0);
	mb.addUVCoords(1, uvs1);
    uint32_t faceIdx = mb.addFace();
    mb.setFaceVertexIndices(faceIdx, vtxIdx);
	mb.setFaceUVIndices(faceIdx, 0, uvIdx0);
	mb.setFaceUVIndices(faceIdx, 1, uvIdx1);
	const auto m = mb.createShared();

	CHECK(m->getUVSetsCount() == 2);

    prtx::GeometryBuilder gb;
    gb.addMesh(m);
    auto geo = gb.createShared();
    const prtx::GeometryPtrVector geos = { geo };

    SerializedGeometry sg;
    serializeGeometry(sg, geos);

    CHECK(sg.counts == faceCnt);
    CHECK(sg.coords == vtx);
	const prtx::IndexVector vtxIdxExp = { 3, 2, 1, 0 };
	CHECK(sg.indices == vtxIdxExp);

	const prtx::DoubleVector uvsExp = { 0.0, 0.0,  1.0, 0.0,  1.0, 1.0,  0.0, 1.0,   0.0, 0.0,  0.5, 0.0,  0.5, 0.5,  0.0, 0.5 };
	CHECK(sg.uvCoords == uvsExp);

	CHECK(sg.uvSets == 2);

	const prtx::IndexVector uvCountsExp = { 4, 4 };
	CHECK(sg.uvCounts == uvCountsExp);

	const prtx::IndexVector uvIdxExp = { 3, 2, 1, 0,  4, 5, 6, 7 };
	CHECK(sg.uvIndices == uvIdxExp);
}

TEST_CASE("serialize mesh with two non-consecutive uv sets") {
	const prtx::IndexVector  faceCnt = { 4 };
	const prtx::DoubleVector vtx     = { 0.0, 0.0, 0.0,  1.0, 0.0, 0.0,  1.0, 0.0, 1.0,  0.0, 0.0, 1.0 };
	const prtx::IndexVector  vtxIdx  = { 0, 1, 2, 3 };
	const prtx::DoubleVector uvs0    = { 0.0, 0.0,  1.0, 0.0,  1.0, 1.0,  0.0, 1.0 };
	const prtx::IndexVector  uvIdx0  = { 0, 1, 2, 3 };
	const prtx::DoubleVector uvs2    = { 0.0, 0.0,  0.5, 0.0,  0.5, 0.5,  0.0, 0.5 };
	const prtx::IndexVector  uvIdx2  = { 3, 2, 1, 0 };

    prtx::MeshBuilder mb;
    mb.addVertexCoords(vtx);
	mb.addUVCoords(0, uvs0);
	mb.addUVCoords(2, uvs2);
    uint32_t faceIdx = mb.addFace();
    mb.setFaceVertexIndices(faceIdx, vtxIdx);
	mb.setFaceUVIndices(faceIdx, 0, uvIdx0);
	mb.setFaceUVIndices(faceIdx, 2, uvIdx2);
	const auto m = mb.createShared();

	CHECK(m->getUVSetsCount() == 4); // TODO: this is wrong!

    prtx::GeometryBuilder gb;
    gb.addMesh(m);
    auto geo = gb.createShared();
    const prtx::GeometryPtrVector geos = { geo };

    SerializedGeometry sg;
    serializeGeometry(sg, geos);

    CHECK(sg.counts == faceCnt);
    CHECK(sg.coords == vtx);
	const prtx::IndexVector vtxIdxExp = { 3, 2, 1, 0 };
	CHECK(sg.indices == vtxIdxExp);

	const prtx::DoubleVector uvsExp = { 0.0, 0.0,  1.0, 0.0,  1.0, 1.0,  0.0, 1.0,   0.0, 0.0,  0.5, 0.0,  0.5, 0.5,  0.0, 0.5 };
	CHECK(sg.uvCoords == uvsExp);

	CHECK(sg.uvSets == 4); // TODO: wrong!

	const prtx::IndexVector uvCountsExp = { 4, 0, 4, 0 };
	CHECK(sg.uvCounts == uvCountsExp);

	const prtx::IndexVector uvIdxExp = { 3, 2, 1, 0,  4, 5, 6, 7 };
	CHECK(sg.uvIndices == uvIdxExp);
}

TEST_CASE("serialize mesh with mixed face uvs (one uv set)") {
	const prtx::IndexVector  faceCnt  = { 4, 3, 5 };
	const prtx::DoubleVector vtx      = { 0.0, 0.0, 0.0,  1.0, 0.0, 0.0,  1.0, 0.0, 1.0,  0.0, 0.0, 1.0,  0.0, 0.0, -1.0 };
	const prtx::IndexVector  vtxIdx[] = {  { 0, 1, 2, 3 }, { 0, 1, 4 }, { 0, 1, 2, 3, 4 } };
	const prtx::DoubleVector uvs      = { 0.0, 0.0,  1.0, 0.0,  1.0, 1.0,  0.0, 1.0 };
	const prtx::IndexVector  uvIdx[]  = { { 0, 1, 2, 3 }, { }, { 0, 1, 2, 3, 0 } }; // face 1 has no uvs

    prtx::MeshBuilder mb;
    mb.addVertexCoords(vtx);
	mb.addUVCoords(0, uvs);

    mb.addFace();
    mb.setFaceVertexIndices(0, vtxIdx[0]);
	mb.setFaceUVIndices(0, 0, uvIdx[0]);

    mb.addFace();
    mb.setFaceVertexIndices(1, vtxIdx[1]);
	mb.setFaceUVIndices(1, 0, uvIdx[1]); // has no uvs

    mb.addFace();
    mb.setFaceVertexIndices(2, vtxIdx[2]);
	mb.setFaceUVIndices(2, 0, uvIdx[2]);

	const auto m = mb.createShared();

	CHECK(m->getUVSetsCount() == 2); // TODO

    prtx::GeometryBuilder gb;
    gb.addMesh(m);
    auto geo = gb.createShared();
    const prtx::GeometryPtrVector geos = { geo };

    SerializedGeometry sg;
    serializeGeometry(sg, geos);

    CHECK(sg.counts == faceCnt);
	CHECK(sg.coords == vtx);

	const prtx::IndexVector vtxIdxExp = { 3, 2, 1, 0,  4, 1, 0,   4, 3, 2, 1, 0 };
	CHECK(sg.indices == vtxIdxExp);

	CHECK(sg.uvCoords == uvs);

	CHECK(sg.uvSets == 2); // TODO

	const prtx::IndexVector uvCountsExp = { 4, 0, 0, 0, 5, 0 };
	CHECK(sg.uvCounts == uvCountsExp);

	const prtx::IndexVector uvIdxExp = { 3, 2, 1, 0,   0, 3, 2, 1, 0 };
	CHECK(sg.uvIndices == uvIdxExp);
}

TEST_CASE("serialize two meshes with one uv set") {
	const prtx::IndexVector faceCnt = {4};
	const prtx::DoubleVector vtx = {0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 1.0, 0.0, 1.0, 0.0, 0.0, 1.0};
	const prtx::IndexVector vtxIdx = {0, 1, 2, 3};
	const prtx::DoubleVector uvs = {0.0, 0.0, 1.0, 0.0, 1.0, 1.0, 0.0, 1.0};
	const prtx::IndexVector uvIdx = {0, 1, 2, 3};

	prtx::MeshBuilder mb;
	mb.addVertexCoords(vtx);
	mb.addUVCoords(0, uvs);
	uint32_t faceIdx = mb.addFace();
	mb.setFaceVertexIndices(faceIdx, vtxIdx);
	mb.setFaceUVIndices(faceIdx, 0, uvIdx);
	const auto m1 = mb.createShared();
	const auto m2 = mb.createShared();

	CHECK(m1->getUVSetsCount() == 2); // TODO
	CHECK(m2->getUVSetsCount() == 2); // TODO

	prtx::GeometryBuilder gb;
	gb.addMesh(m1);
	gb.addMesh(m2);
	auto geo = gb.createShared();
	const prtx::GeometryPtrVector geos = {geo};

	SerializedGeometry sg;
	serializeGeometry(sg, geos);

	const prtx::IndexVector expFaceCnt = {4, 4};
	CHECK(sg.counts == expFaceCnt);

	const prtx::DoubleVector expVtx = { 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 1.0, 0.0, 1.0, 0.0, 0.0, 1.0,
	                                    0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 1.0, 0.0, 1.0, 0.0, 0.0, 1.0 };
	CHECK(sg.coords == expVtx);

	const prtx::IndexVector expVtxIdx = { 3, 2, 1, 0, 7, 6, 5, 4 };
	CHECK(sg.indices == expVtxIdx);

	const prtx::DoubleVector expUvs = { 0.0, 0.0, 1.0, 0.0, 1.0, 1.0, 0.0, 1.0,
	                                    0.0, 0.0, 1.0, 0.0, 1.0, 1.0, 0.0, 1.0 };
	CHECK(sg.uvCoords == expUvs);

	CHECK(sg.uvSets == 2); // TODO: actually wrong expected value

	const prtx::IndexVector uvCountsExp = { 4, 0, 4, 0 };
	CHECK(sg.uvCounts == uvCountsExp);


	const prtx::IndexVector expUvIdx = { 3, 2, 1, 0,   7, 6, 5, 4 };
	CHECK(sg.uvIndices == expUvIdx);
}


struct GenerateData { // TODO: could use ShapeData from production code
	InitialShapeBuilderVector         mInitialShapeBuilders;
	InitialShapeNOPtrVector           mInitialShapes;

	AttributeMapBuilderVector         mRuleAttributeBuilders;
	AttributeMapVector                mRuleAttributes;

	~GenerateData() {
		std::for_each(mInitialShapes.begin(), mInitialShapes.end(), [](const prt::InitialShape* is){
			assert(is != nullptr);
			is->destroy();
		});
	}
};

void enumerateFiles(boost::filesystem::path const& root, std::vector<boost::filesystem::path>& paths) {
	namespace fs = boost::filesystem;
	for (fs::recursive_directory_iterator file(root); file != fs::recursive_directory_iterator(); ++file) {
		if (!fs::is_directory(file->path()))
			paths.push_back(file->path());
	}
}

ResolveMapUPtr createResolveMap(const boost::filesystem::path& d) { // TODO: unify with PRTContext::getResolveMap
	std::vector<boost::filesystem::path> paths;
	enumerateFiles(d, paths);

	ResolveMapBuilderUPtr rmb{prt::ResolveMapBuilder::create()};
	for(const auto& p: paths) {
		std::wstring key = p.generic_wstring();
		prtx::URIPtr uri = prtx::URIUtils::createFileURI(prtx::URIUtils::percentEncode(p.generic_wstring()));
		rmb->addEntry(key.c_str(), uri->wstring().c_str());
	}
	return ResolveMapUPtr{rmb->createResolveMap()};
}

struct CallbackResult {
	std::wstring name;
	std::vector<double> vtx;
	std::vector<double> nrm;
	std::vector<double> uvs;
	std::vector<uint32_t> cnts;
	std::vector<uint32_t> idx;
	std::vector<uint32_t> uvCnts;
	std::vector<uint32_t> uvIdx;
	uint32_t uvSets;
	std::vector<AttributeMapUPtr> materials;
	std::vector<uint32_t> faceRanges;
};

class TestCallbacks : public HoudiniCallbacks {
public:
	std::vector<CallbackResult> results;


void add(const wchar_t* name,
         const double* vtx, size_t vtxSize,
         const double* nrm, size_t nrmSize,
         const double* uvs, size_t uvsSize,
         const uint32_t* counts, size_t countsSize,
         const uint32_t* indices, size_t indicesSize,
         const uint32_t* uvCounts, size_t uvCountsSize,
         const uint32_t* uvIndices, size_t uvIndicesSize,
         uint32_t uvSets,
         const uint32_t* faceRanges, size_t faceRangesSize,
         const prt::AttributeMap** materials,
         const prt::AttributeMap** reports
) {
	results.emplace_back(CallbackResult());
	auto& cr = results.back();

	cr.name = name;
	cr.vtx.assign(vtx, vtx+vtxSize);
	cr.nrm.assign(nrm, nrm+nrmSize);
	cr.uvs.assign(uvs, uvs+uvsSize);
	cr.cnts.assign(counts, counts+countsSize);
	cr.idx.assign(indices, indices+indicesSize);
	cr.uvCnts.assign(uvCounts, uvCounts+uvCountsSize);
	cr.uvIdx.assign(uvIndices, uvIndices+uvIndicesSize);
	cr.uvSets = uvSets;

	for (size_t mi = 0; mi < faceRangesSize-1; mi++) {
		AttributeMapBuilderUPtr amb(prt::AttributeMapBuilder::createFromAttributeMap(materials[mi]));
		cr.materials.emplace_back(amb->createAttributeMap());
	}

	cr.faceRanges.assign(faceRanges, faceRanges+faceRangesSize); // faceRanges contains materialSize+1 values

}

prt::Status generateError(size_t isIndex, prt::Status status, const wchar_t* message) override {
	return prt::STATUS_OK;
}

prt::Status assetError(size_t isIndex, prt::CGAErrorLevel level, const wchar_t* key, const wchar_t* uri, const wchar_t* message) override {
	return prt::STATUS_OK;
}

prt::Status cgaError(size_t isIndex, int32_t shapeID, prt::CGAErrorLevel level, int32_t methodId, int32_t pc, const wchar_t* message) override {
	return prt::STATUS_OK;
}

prt::Status cgaPrint(size_t isIndex, int32_t shapeID, const wchar_t* txt) override {
	return prt::STATUS_OK;
}

prt::Status cgaReportBool(size_t isIndex, int32_t shapeID, const wchar_t* key, bool value) override {
	return prt::STATUS_OK;
}

prt::Status cgaReportFloat(size_t isIndex, int32_t shapeID, const wchar_t* key, double value) override {
	return prt::STATUS_OK;
}

prt::Status cgaReportString(size_t isIndex, int32_t shapeID, const wchar_t* key, const wchar_t* value) override {
	return prt::STATUS_OK;
}

prt::Status attrBool(size_t isIndex, int32_t shapeID, const wchar_t* key, bool value) override {
	return prt::STATUS_OK;
}

prt::Status attrFloat(size_t isIndex, int32_t shapeID, const wchar_t* key, double value) override {
	return prt::STATUS_OK;
}

prt::Status attrString(size_t isIndex, int32_t shapeID, const wchar_t* key, const wchar_t* value) override {
	return prt::STATUS_OK;
}

};


TEST_CASE("generate two cubes with two uv sets") {
	const boost::filesystem::path testData = "/home/shaegler/esri/dev/git/houdini-cityengine-plugin/src/test/data"; // TODO

	const std::vector<boost::filesystem::path> initialShapeSources = {
	testData / "quad0.obj",
	testData / "quad1.obj"
	};

	const std::vector<std::wstring> initialShapeURIs = {
	toFileURI(initialShapeSources[0]),
	toFileURI(initialShapeSources[1])
	};

	const std::vector<std::wstring> startRules = {
	L"Default$OneSet",
	L"Default$TwoSets"
	};

	const boost::filesystem::path rpkPath = testData / "uvsets.rpk";
	const ResolveMapUPtr& rpkRM = prtCtx->getResolveMap(rpkPath);

	GenerateData gd;
	REQUIRE(initialShapeURIs.size() == startRules.size());
	for (size_t i = 0; i < initialShapeURIs.size(); i++) {
		const std::wstring& uri = initialShapeURIs[i];
		const std::wstring& sr = startRules[i];

		gd.mInitialShapeBuilders.emplace_back(prt::InitialShapeBuilder::create());
		auto& isb = gd.mInitialShapeBuilders.back();

		gd.mRuleAttributeBuilders.emplace_back(prt::AttributeMapBuilder::create());
		auto& amb = gd.mRuleAttributeBuilders.back();
		gd.mRuleAttributes.emplace_back(amb->createAttributeMap());
		const auto& am = gd.mRuleAttributes.back();

		const std::wstring sn = L"shape" + std::to_wstring(i);

		REQUIRE(isb->resolveGeometry(uri.c_str()) == prt::STATUS_OK);
		REQUIRE(isb->setAttributes(L"bin/r1.cgb", sr.c_str(), 0, sn.c_str(), am.get(), rpkRM.get()) == prt::STATUS_OK);

		prt::Status status = prt::STATUS_UNSPECIFIED_ERROR;
		const prt::InitialShape* is = isb->createInitialShapeAndReset(&status);
		REQUIRE(status == prt::STATUS_OK);
		gd.mInitialShapes.emplace_back(is);
	}

	constexpr const wchar_t* ENCODER_ID_CGA_ERROR = L"com.esri.prt.core.CGAErrorEncoder";
	constexpr const wchar_t* ENCODER_ID_CGA_PRINT = L"com.esri.prt.core.CGAPrintEncoder";
	constexpr const wchar_t* ENCODER_ID_HOUDINI = L"HoudiniEncoder";
	constexpr const wchar_t* FILE_CGA_ERROR = L"CGAErrors.txt";
	constexpr const wchar_t* FILE_CGA_PRINT = L"CGAPrint.txt";

	AttributeMapBuilderUPtr amb(prt::AttributeMapBuilder::create());

	amb->setBool(L"emitAttributes", true);
	amb->setBool(L"emitMaterials", true);
	amb->setBool(L"emitReports", true);
	const AttributeMapUPtr rawEncOpts(amb->createAttributeMapAndReset());
	const AttributeMapUPtr houdiniEncOpts(createValidatedOptions(ENCODER_ID_HOUDINI, rawEncOpts.get()));

	amb->setString(L"name", FILE_CGA_ERROR);
	const AttributeMapUPtr errOptions(amb->createAttributeMapAndReset());
	const AttributeMapUPtr cgaErrorOptions(createValidatedOptions(ENCODER_ID_CGA_ERROR, errOptions.get()));

	amb->setString(L"name", FILE_CGA_PRINT);
	const AttributeMapUPtr printOptions(amb->createAttributeMapAndReset());
	const AttributeMapUPtr cgaPrintOptions(createValidatedOptions(ENCODER_ID_CGA_PRINT, printOptions.get()));

	amb->setInt(L"numberWorkerThreads", prtCtx->mCores);
	const AttributeMapUPtr generateOptions(amb->createAttributeMapAndReset());

	const std::vector<const wchar_t*> allEncoders = { ENCODER_ID_HOUDINI, ENCODER_ID_CGA_ERROR, ENCODER_ID_CGA_PRINT };
	const AttributeMapNOPtrVector allEncoderOptions = { houdiniEncOpts.get(), cgaErrorOptions.get(),
	                                                    cgaPrintOptions.get() };

	TestCallbacks tc;

	prt::Status stat = prt::generate(gd.mInitialShapes.data(), gd.mInitialShapes.size(), nullptr,
	                                 allEncoders.data(), allEncoders.size(), allEncoderOptions.data(), &tc,
	                                 prtCtx->mPRTCache.get(), nullptr, generateOptions.get());
	REQUIRE(stat == prt::STATUS_OK);

	REQUIRE(tc.results.size() == 2);

	// TODO: also check actual coordinates etc

	{
		const CallbackResult& cr = tc.results[0];
		CHECK(cr.name == L"shape0");

		const std::vector<uint32_t> cntsExp = { 4, 4, 4, 4, 4, 4 };
		CHECK(cr.cnts == cntsExp);

		const std::vector<uint32_t> idxExp = { 3, 2, 1, 0, 7, 6, 5, 4, 11, 10, 9, 8, 15, 14, 13, 12, 19, 18, 17, 16, 23, 22, 21, 20 };
		CHECK(cr.idx == idxExp);

		const std::vector<uint32_t> uvCntsExp = { 4, 0, 4, 0, 4, 0, 4, 0, 4, 0, 4, 0 };
		CHECK(cr.uvCnts == uvCntsExp);

		const std::vector<uint32_t> uvIdxExp = { 3, 2, 1, 0, 7, 6, 5, 4, 11, 10, 9, 8, 15, 14, 13, 12, 19, 18, 17, 16, 23, 22, 21, 20 };
		CHECK(cr.uvIdx == uvIdxExp);

		CHECK(cr.uvSets == 2);

		const std::vector<uint32_t> faceRangesExp = { 0, 6 };
		CHECK(cr.faceRanges == faceRangesExp);
	}

	{
		const CallbackResult& cr = tc.results[1];
		CHECK(cr.name == L"shape1");

		const std::vector<uint32_t> cntsExp = { 4, 4, 4, 4, 4, 4 };
		CHECK(cr.cnts == cntsExp);

		const std::vector<uint32_t> idxExp = { 3, 2, 1, 0, 7, 6, 5, 4, 11, 10, 9, 8,
		                                       15, 14, 13, 12, 19, 18, 17, 16, 23, 22, 21, 20 };
		CHECK(cr.idx == idxExp);

		const std::vector<uint32_t> uvCntsExp = { 4, 0, 4, 0, 4, 0, 4, 0, 4, 0, 4, 0,
		                                          4, 0, 4, 0, 4, 0, 4, 0, 4, 0, 4, 0 };
		CHECK(cr.uvCnts == uvCntsExp);

		const std::vector<uint32_t> uvIdxExp = { 3, 2, 1, 0, 7, 6, 5, 4, 11, 10, 9, 8, 15, 14, 13, 12, 19, 18, 17, 16, 23,
		                                         22, 21, 20, 27, 26, 25, 24, 31, 30, 29, 28, 35, 34, 33, 32, 39, 38, 37, 36,
		                                         43, 42, 41, 40, 47, 46, 45, 44 };
		CHECK(cr.uvIdx == uvIdxExp);

		CHECK(cr.uvSets == 4);

		const std::vector<uint32_t> faceRangesExp = { 0, 1, 2, 3, 4, 5, 6 };
		CHECK(cr.faceRanges == faceRangesExp);
	}

}
