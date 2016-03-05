#include "client/callbacks.h"

#include "GU/GU_HoleInfo.h"


namespace {
const bool DBG = true;
}

namespace p4h {

HoudiniGeometry::HoudiniGeometry(GU_Detail* gdp, prt::AttributeMapBuilder* eab)
: mDetail(gdp), mEvalAttrBuilder(eab), mCurOffset(0) { }

void HoudiniGeometry::setVertices(double* vtx, size_t size) {
	mPoints.reserve(size / 3);
	for (size_t pi = 0; pi < size; pi += 3)
		mPoints.push_back(UT_Vector3(vtx[pi], vtx[pi+1], vtx[pi+2]));
}

void HoudiniGeometry::setNormals(double* nrm, size_t size) {
	// TODO
}

void HoudiniGeometry::setUVs(float* u, float* v, size_t size) {
	//		GA_RWHandleV3 txth = GA_RWHandleV3(mDetails->addTextureAttribute(GA_ATTRIB_PRIMITIVE));
	// TODO
}

void HoudiniGeometry::setFaces(
	int32_t* counts, size_t countsSize,
	int32_t* connects, size_t connectsSize,
	int32_t* uvCounts, size_t uvCountsSize,
	int32_t* uvConnects, size_t uvConnectsSize,
	int32_t* holes, size_t holesSize
) {
	for (size_t ci = 0; ci < countsSize; ci++)
		mPolyCounts.append(counts[ci]);
	mIndices.reserve(connectsSize);
	mIndices.insert(mIndices.end(), connects, connects+connectsSize);
	if (holes && holesSize > 0)
		mHoles.insert(mHoles.end(), holes, holes+holesSize);
}

void HoudiniGeometry::createMesh(const wchar_t* name) {
	std::string nname = p4h::utils::toOSNarrowFromUTF16(name);
	mCurGroup = static_cast<GA_PrimitiveGroup*>(mDetail->getElementGroupTable(GA_ATTRIB_PRIMITIVE).newGroup(nname.c_str(), false));
	if (DBG) LOG_DBG << "createMesh: " << nname;
}

void HoudiniGeometry::matSetColor(int start, int count, float r, float g, float b) {
	LOG_DBG << "matSetColor: start = " << start << ", count = " << count << ", rgb = " << r << ", " << g << ", " << b;
	GA_Offset off = mCurOffset + start;
	GA_RWHandleV3 c(mDetail->addDiffuseAttribute(GA_ATTRIB_PRIMITIVE));
	UT_Vector3 color(r, g, b);
	for (int i = 0; i < count; i++) {
		c.set(off++, color);
	}
	LOG_DBG << "matSetColor done";
}

void HoudiniGeometry::matSetDiffuseTexture(int start, int count, const wchar_t* tex) {
	//		LOG_DBG << L"matSetDiffuseTexture: " << start << L", count = " << count << L", tex = " << tex;
	//		GA_RWHandleV3 txth = GA_RWHandleV3(mDetails->addTextureAttribute(GA_ATTRIB_PRIMITIVE));
	//		GA_Offset off = mCurOffset + start;
	//		for (int i = 0; i < count; i++)
	//			txth.set(off++, )
}

void HoudiniGeometry::finishMesh() {
	if (DBG) LOG_DBG << "finishMesh begin";
	GA_IndexMap::Marker marker(mDetail->getPrimitiveMap());
	mCurOffset = GU_PrimPoly::buildBlock(mDetail, &mPoints[0], mPoints.size(), mPolyCounts, &mIndices[0]);

#if 0 // keep disabled until hole support for initial shapes is completed
	static const int32_t HOLE_DELIM = std::numeric_limits<int32_t>::max();
	std::vector<int32_t> holeFaceIndices;
	int32_t faceIdx = 0;
	for (size_t hi = 0; hi < mHoles.size(); hi++) {
		if (mHoles[hi] == HOLE_DELIM) {
			GEO_Primitive* holeOwner = mDetail->getGEOPrimitiveByIndex(faceIdx);
			for (int32_t hfi: holeFaceIndices) {
				GEO_Primitive* holePrim = mDetail->getGEOPrimitiveByIndex(hfi);
				GU_HoleInfo holeInfo(holePrim);
				holeInfo.setPromotedFace(static_cast<GEO_Face*>(holeOwner));
				holeInfo.setReversed();
			}
			holeFaceIndices.clear();
			faceIdx++;
			continue;
		}
		holeFaceIndices.push_back(mHoles[hi]);
	}
#endif

	mCurGroup->addRange(marker.getRange());
	mPolyCounts.clear();
	mIndices.clear();
	mPoints.clear();
	if (DBG) LOG_DBG << "finishMesh done";
}

prt::Status HoudiniGeometry::generateError(size_t isIndex, prt::Status status, const wchar_t* message) {
	LOG_ERR << message;
	return prt::STATUS_OK;
}
prt::Status HoudiniGeometry::assetError(size_t isIndex, prt::CGAErrorLevel level, const wchar_t* key, const wchar_t* uri, const wchar_t* message) {
	LOG_WRN << key << L": " << message;
	return prt::STATUS_OK;
}
prt::Status HoudiniGeometry::cgaError(size_t isIndex, int32_t shapeID, prt::CGAErrorLevel level, int32_t methodId, int32_t pc, const wchar_t* message) {
	LOG_ERR << message;
	return prt::STATUS_OK;
}
prt::Status HoudiniGeometry::cgaPrint(size_t isIndex, int32_t shapeID, const wchar_t* txt) {
	return prt::STATUS_OK;
}
prt::Status HoudiniGeometry::cgaReportBool(size_t isIndex, int32_t shapeID, const wchar_t* key, bool value) {
	return prt::STATUS_OK;
}
prt::Status HoudiniGeometry::cgaReportFloat(size_t isIndex, int32_t shapeID, const wchar_t* key, double value) {
	return prt::STATUS_OK;
}
prt::Status HoudiniGeometry::cgaReportString(size_t isIndex, int32_t shapeID, const wchar_t* key, const wchar_t* value) {
	return prt::STATUS_OK;
}

prt::Status HoudiniGeometry::attrBool(size_t isIndex, int32_t shapeID, const wchar_t* key, bool value) {
	if (mEvalAttrBuilder != nullptr) {
		mEvalAttrBuilder->setBool(key, value);
	}
	return prt::STATUS_OK;
}

prt::Status HoudiniGeometry::attrFloat(size_t isIndex, int32_t shapeID, const wchar_t* key, double value) {
	if (mEvalAttrBuilder != nullptr) {
		mEvalAttrBuilder->setFloat(key, value);
	}
	return prt::STATUS_OK;
}

prt::Status HoudiniGeometry::attrString(size_t isIndex, int32_t shapeID, const wchar_t* key, const wchar_t* value) {
	if (mEvalAttrBuilder != nullptr) {
		mEvalAttrBuilder->setString(key, value);
	}
	return prt::STATUS_OK;
}

}
