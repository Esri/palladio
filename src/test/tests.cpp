/*
 * Copyright 2014-2018 Esri R&D Zurich and VRBN
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "TestCallbacks.h"
#include "TestUtils.h"

#include "../palladio/PRTContext.h"
#include "../palladio/Utils.h"
#include "../palladio/ModelConverter.h"
#include "../palladio/AttributeConversion.h"
#include "../codec/encoder/HoudiniEncoder.h"

#include "prt/AttributeMap.h"
#include "prtx/Geometry.h"
#include "prtx/Mesh.h"

#include "../palladio/BoostRedirect.h"
#include PLD_BOOST_INCLUDE(/filesystem/path.hpp)

#define CATCH_CONFIG_RUNNER
#include "catch.hpp"

#include <algorithm>


namespace {

PRTContextUPtr prtCtx;
const PLD_BOOST_NS::filesystem::path testDataPath = TEST_DATA_PATH;

} // namespace


int main( int argc, char* argv[] ) {
	assert(!prtCtx);

	const std::vector<PLD_BOOST_NS::filesystem::path> addExtDirs = {
		//"../lib", // adapt to default prt dir layout (core is in bin subdir)
		HOUDINI_CODEC_PATH // set to absolute path to houdini encoder lib via cmake
	};

	prtCtx.reset(new PRTContext(addExtDirs));
	int result = Catch::Session().run( argc, argv );
	prtCtx.reset();
	return result;
}


TEST_CASE("create file URI from path", "[utils]" ) {
#ifdef PLD_LINUX
	const auto u = toFileURI(PLD_BOOST_NS::filesystem::path("/tmp/foo.txt"));
    CHECK(u.compare(L"file:/tmp/foo.txt") == 0);
#elif defined(PLD_WINDOWS)
	const auto u = toFileURI(PLD_BOOST_NS::filesystem::path("c:\\tmp\\foo.txt"));
    INFO(toOSNarrowFromUTF16(u));
    CHECK(u.compare(L"file:/c:/tmp/foo.txt") == 0);
#endif
}

TEST_CASE("percent-encode a UTF-8 string", "[utils]") {
    CHECK(percentEncode("with space") == L"with%20space");
}

TEST_CASE("get XML representation of a PRT object", "[utils]") {
    AttributeMapBuilderUPtr amb(prt::AttributeMapBuilder::create());
	amb->setString(L"foo", L"bar");
    AttributeMapUPtr am(amb->createAttributeMap());
    std::string xml = objectToXML(am.get());
    CHECK(xml == "<attributable>\n\t<attribute key=\"foo\" value=\"bar\" type=\"str\"/>\n</attributable>"); // TODO: use R?
}

TEST_CASE("replace chars not in set", "[utils]") {
	const std::wstring ac = L"abc";

	SECTION("basic") {
		std::wstring s = L"abc_def";
		replace_all_not_of(s, ac);
		CHECK(s == L"abc____");
	}

	SECTION("empty") {
		std::wstring s = L"";
		replace_all_not_of(s, ac);
		CHECK(s == L"");
	}

	SECTION("one char") {
		std::wstring s = L" ";
		replace_all_not_of(s, ac);
		CHECK(s == L"_");
	}

	SECTION("one allowed char") {
		std::wstring s = L"_";
		replace_all_not_of(s, ac);
		CHECK(s == L"_");
	}
}

TEST_CASE("separate fully qualified name into style and name", "[NameConversion]") {
	std::wstring style, name;

	SECTION("default case") {
		NameConversion::separate(L"foo$bar", style, name);
		CHECK(style == L"foo");
		CHECK(name == L"bar");
	}

	SECTION("no style") {
		NameConversion::separate(L"foo", style, name);
		CHECK(style.empty());
		CHECK(name == L"foo");
	}

	SECTION("edge case 1") {
		NameConversion::separate(L"foo$", style, name);
		CHECK(style == L"foo");
		CHECK(name.empty());
	}

	SECTION("edge case 2") {
		NameConversion::separate(L"$foo", style, name);
		CHECK(style.empty());
		CHECK(name == L"foo");
	}

	SECTION("separator only") {
		NameConversion::separate(L"$", style, name);
		CHECK(style.empty());
		CHECK(name.empty());
	}

	SECTION("empty") {
		NameConversion::separate(L"", style, name);
		CHECK(style.empty());
		CHECK(name.empty());
	}

	SECTION("two separators") {
		NameConversion::separate(L"foo$bar$baz", style, name);
		CHECK(style == L"foo");
		CHECK(name == L"bar$baz");
	}

}


// -- encoder test cases

TEST_CASE("serialize basic mesh") {
	const std::vector<uint32_t>  faceCnt   = { 4 };
	const prtx::DoubleVector vtx       = { 0.0, 0.0, 0.0,  1.0, 0.0, 0.0,  1.0, 0.0, 1.0,  0.0, 0.0, 1.0 };
	const std::vector<uint32_t> vtxInd = { 0, 1, 2, 3 };
	const std::vector<uint32_t> vtxIndRev = [&vtxInd]() { auto c = vtxInd; std::reverse_copy(vtxInd.begin(), vtxInd.end(), c.begin()); return c; }();

    prtx::MeshBuilder mb;
    mb.addVertexCoords(vtx);
    uint32_t faceIdx = mb.addFace();
    mb.setFaceVertexIndices(faceIdx, vtxInd);

    prtx::GeometryBuilder gb;
    gb.addMesh(mb.createShared());

    auto geo = gb.createShared();
	CHECK(geo->getMeshes().front()->getUVSetsCount() == 0);

    const prtx::GeometryPtrVector geos = { geo };
    const std::vector<prtx::MaterialPtrVector> mats = { geo->getMeshes().front()->getMaterials() };

	const detail::SerializedGeometry sg = detail::serializeGeometry(geos, mats);

    CHECK(sg.counts == faceCnt);
	CHECK(sg.coords == vtx);
	CHECK(sg.indices == vtxIndRev); // reverses winding

	CHECK(sg.uvs.size() == 0);
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

#if PRT_VERSION_MAJOR < 2
	CHECK(m->getUVSetsCount() == 2); // bug in 1.x
#else
	CHECK(m->getUVSetsCount() == 1);
#endif

    prtx::GeometryBuilder gb;
    gb.addMesh(m);
    auto geo = gb.createShared();
    const prtx::GeometryPtrVector geos = { geo };
    const std::vector<prtx::MaterialPtrVector> mats = { m->getMaterials() };

	const detail::SerializedGeometry sg = detail::serializeGeometry(geos, mats);

    CHECK(sg.counts == faceCnt);
    CHECK(sg.coords == vtx);
	const prtx::IndexVector vtxIdxExp = { 3, 2, 1, 0 };
	CHECK(sg.indices == vtxIdxExp);

#if PRT_VERSION_MAJOR < 2
	CHECK(sg.uvs.size() == 2); // bug in 1.x
#else
	CHECK(sg.uvs.size() == 1);
#endif
	CHECK(sg.uvs[0] == uvs);
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
    const std::vector<prtx::MaterialPtrVector> mats = { m->getMaterials() };

	const detail::SerializedGeometry sg = detail::serializeGeometry(geos, mats);

    CHECK(sg.counts == faceCnt);
    CHECK(sg.coords == vtx);
	const prtx::IndexVector vtxIdxExp = { 3, 2, 1, 0 };
	CHECK(sg.indices == vtxIdxExp);

	CHECK(sg.uvs.size() == 2);

	const prtx::DoubleVector uvs0Exp = { 0.0, 0.0,  1.0, 0.0,  1.0, 1.0,  0.0, 1.0 };
	CHECK(sg.uvs[0] == uvs0Exp);
	const prtx::DoubleVector uvs1Exp = { 0.0, 0.0,  0.5, 0.0,  0.5, 0.5,  0.0, 0.5 };
	CHECK(sg.uvs[1] == uvs1Exp);
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

#if PRT_VERSION_MAJOR < 2
	CHECK(m->getUVSetsCount() == 4); // bug in 1.x
#else
	CHECK(m->getUVSetsCount() == 3);
#endif

    prtx::GeometryBuilder gb;
    gb.addMesh(m);
    auto geo = gb.createShared();
    const prtx::GeometryPtrVector geos = { geo };
    const std::vector<prtx::MaterialPtrVector> mats = { m->getMaterials() };

	const detail::SerializedGeometry sg = detail::serializeGeometry(geos, mats);

    CHECK(sg.counts == faceCnt);
    CHECK(sg.coords == vtx);
	const prtx::IndexVector vtxIdxExp = { 3, 2, 1, 0 };
	CHECK(sg.indices == vtxIdxExp);

#if PRT_VERSION_MAJOR < 2
	CHECK(sg.uvs.size() == 4); // bug in 1.x
#else
	CHECK(sg.uvs.size() == 3);
#endif

	const prtx::DoubleVector uvs0Exp = { 0.0, 0.0,  1.0, 0.0,  1.0, 1.0,  0.0, 1.0 };
	CHECK(sg.uvs[0] == uvs0Exp);
	CHECK(sg.uvs[1] == uvs0Exp); // fallback to uv set 0
	const prtx::DoubleVector uvs2Exp = { 0.0, 0.0,  0.5, 0.0,  0.5, 0.5,  0.0, 0.5 };
	CHECK(sg.uvs[2] == uvs2Exp);
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

#if PRT_VERSION_MAJOR < 2
	CHECK(m->getUVSetsCount() == 2); // bug in 1.x
#else
	CHECK(m->getUVSetsCount() == 1);
#endif

    prtx::GeometryBuilder gb;
    gb.addMesh(m);
    auto geo = gb.createShared();
    const prtx::GeometryPtrVector geos = { geo };
    const std::vector<prtx::MaterialPtrVector> mats = { m->getMaterials() };

	const detail::SerializedGeometry sg = detail::serializeGeometry(geos, mats);

    CHECK(sg.counts == faceCnt);
	CHECK(sg.coords == vtx);

	const prtx::IndexVector vtxIdxExp = { 3, 2, 1, 0,  4, 1, 0,   4, 3, 2, 1, 0 };
	CHECK(sg.indices == vtxIdxExp);

#if PRT_VERSION_MAJOR < 2
	CHECK(sg.uvs.size() == 2); // bug in 1.x
#else
	CHECK(sg.uvs.size() == 1);
#endif
	CHECK(sg.uvs[0] == uvs);

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

#if PRT_VERSION_MAJOR < 2
	CHECK(m1->getUVSetsCount() == 2); // bug in 1.x
	CHECK(m2->getUVSetsCount() == 2);
#else
	CHECK(m1->getUVSetsCount() == 1);
	CHECK(m2->getUVSetsCount() == 1);
#endif

	prtx::GeometryBuilder gb;
	gb.addMesh(m1);
	gb.addMesh(m2);
	auto geo = gb.createShared();
	const prtx::GeometryPtrVector geos = {geo};
    const std::vector<prtx::MaterialPtrVector> mats = { { geo->getMeshes()[0]->getMaterials().front(), geo->getMeshes()[1]->getMaterials().front() } };

	const detail::SerializedGeometry sg = detail::serializeGeometry(geos, mats);

	const prtx::IndexVector expFaceCnt = {4, 4};
	CHECK(sg.counts == expFaceCnt);

	const prtx::DoubleVector expVtx = { 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 1.0, 0.0, 1.0, 0.0, 0.0, 1.0,
	                                    0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 1.0, 0.0, 1.0, 0.0, 0.0, 1.0 };
	CHECK(sg.coords == expVtx);

	const prtx::IndexVector expVtxIdx = { 3, 2, 1, 0, 7, 6, 5, 4 };
	CHECK(sg.indices == expVtxIdx);

#if PRT_VERSION_MAJOR < 2
	CHECK(sg.uvs.size() == 2); // bug in 1.x
#else
	CHECK(sg.uvs.size() == 1);
#endif

	const prtx::DoubleVector expUvs = { 0.0, 0.0, 1.0, 0.0, 1.0, 1.0, 0.0, 1.0,
	                                    0.0, 0.0, 1.0, 0.0, 1.0, 1.0, 0.0, 1.0 };
	CHECK(sg.uvs[0] == expUvs);
}

TEST_CASE("generate two cubes with two uv sets") {
	const std::vector<PLD_BOOST_NS::filesystem::path> initialShapeSources = {
		testDataPath / "quad0.obj",
		testDataPath / "quad1.obj"
	};

	const std::vector<std::wstring> initialShapeURIs = {
		toFileURI(initialShapeSources[0]),
		toFileURI(initialShapeSources[1])
	};

	const std::vector<std::wstring> startRules = {
		L"Default$OneSet",
		L"Default$TwoSets"
	};

	const PLD_BOOST_NS::filesystem::path rpkPath = testDataPath / "uvsets.rpk";
	const std::wstring ruleFile = L"bin/r1.cgb";

	TestCallbacks tc;
	generate(tc, prtCtx, rpkPath, ruleFile, initialShapeURIs, startRules);
	REQUIRE(tc.results.size() == 2);

	// TODO: also check actual coordinates etc

	{
		const CallbackResult& cr = tc.results[0];
		CHECK(cr.name == L"shape0");

		const std::vector<uint32_t> cntsExp = { 4, 4, 4, 4, 4, 4 };
		CHECK(cr.cnts == cntsExp);

		const std::vector<uint32_t> idxExp = { 3, 2, 1, 0, 7, 6, 5, 4, 11, 10, 9, 8, 15, 14, 13, 12, 19, 18, 17, 16, 23, 22, 21, 20 };
		CHECK(cr.idx == idxExp);

#if PRT_VERSION_MAJOR < 2
		CHECK(cr.uvs.size() == 2); // bug in 1.x
#else
		CHECK(cr.uvs.size() == 1);
#endif

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

#if PRT_VERSION_MAJOR < 2
		CHECK(cr.uvs.size() == 4); // bug in 1.x
#else
		CHECK(cr.uvs.size() == 3);
#endif

		const std::vector<uint32_t> faceRangesExp = { 0, 1, 2, 3, 4, 5, 6 };
		CHECK(cr.faceRanges == faceRangesExp);
	}
}

TEST_CASE("generate with generic attributes") {
	const std::vector<PLD_BOOST_NS::filesystem::path> initialShapeSources = { testDataPath / "quad0.obj" };
	const std::vector<std::wstring> initialShapeURIs = { toFileURI(initialShapeSources[0]) };
	const std::vector<std::wstring> startRules = { L"Default$Init" };
	const PLD_BOOST_NS::filesystem::path rpkPath = testDataPath / "GenAttrs1.rpk";
	const std::wstring ruleFile = L"bin/r1.cgb";

	TestCallbacks tc;
	generate(tc, prtCtx, rpkPath, ruleFile, initialShapeURIs, startRules);

	REQUIRE(tc.results.size() == 1);
	const CallbackResult& cr = tc.results[0];

	const std::vector<uint32_t> faceRangesExp = { 0, 6, 12 };
	CHECK(cr.faceRanges == faceRangesExp);

	REQUIRE(cr.attrsPerShapeID.size() == 2);
	{
		const auto& a = cr.attrsPerShapeID.at(4);
		CHECK(std::wcscmp(a->getString(L"Default$foo"), L"bar") == 0);
	}
	{
		const auto& a = cr.attrsPerShapeID.at(5);
		CHECK(std::wcscmp(a->getString(L"Default$foo"), L"baz") == 0);
	}
}