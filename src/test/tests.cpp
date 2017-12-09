#include "../client/utils.h"
#include "../client/ModelConverter.h"
#include "../codec/encoder/HoudiniEncoder.h"

#include "prt/AttributeMap.h"
#include "prtx/Geometry.h"

#include "boost/filesystem/path.hpp"

#define CATCH_CONFIG_MAIN
#include "catch.hpp"

#include <algorithm>


// -- client test cases

TEST_CASE("create file URI from path", "[utils]" ) {
    CHECK(toFileURI(boost::filesystem::path("/tmp/foo.txt")) == L"file:/tmp/foo.txt");
}

TEST_CASE("percent-encode a UTF-8 string", "[utils]") {
    CHECK(percentEncode("with space") == L"with%20space");
}

TEST_CASE("get XML representation of a PRT object") {
    AttributeMapBuilderUPtr amb(prt::AttributeMapBuilder::create()); // TODO: AttributeMapBuilder::operator<< is broken, report
	amb->setString(L"foo", L"bar");
    AttributeMapUPtr am(amb->createAttributeMap());
    std::string xml = objectToXML(am.get());
    CHECK(xml == "<attributable>\n\t<attribute key=\"foo\" value=\"bar\" type=\"str\"/>\n</attributable>");
}

// TODO: extend for multiple faces
TEST_CASE("deserialize uv set") {
	const prtx::IndexVector faceCnt  = { 4 };
	const prtx::IndexVector uvCounts = { 4, 4 };
	const prtx::IndexVector uvIdx    = { 3, 2, 1, 0,  4, 5, 6, 7 };

	{
		std::vector<uint32_t> uvIndicesPerSet;
		ModelConversion::getUVSet(uvIndicesPerSet, faceCnt.data(), faceCnt.size(), uvCounts.data(), uvCounts.size(), 0, 2, uvIdx.data(), uvIdx.size());
		const prtx::IndexVector uvIndicesPerSetExp = { 3, 2, 1, 0 };
		CHECK(uvIndicesPerSet == uvIndicesPerSetExp);
	}

	{
		std::vector<uint32_t> uvIndicesPerSet;
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