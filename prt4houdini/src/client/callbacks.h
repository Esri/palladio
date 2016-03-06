#pragma once

#include "client/logging.h"
#include "client/utils.h"
#include "codec/encoder/HoudiniCallbacks.h"

#include "prt/AttributeMap.h"
#include "prt/Callbacks.h"

#ifdef P4H_TC_GCC
#	pragma GCC diagnostic push
#	pragma GCC diagnostic ignored "-Wunused-local-typedefs"
#endif

#include "GEO/GEO_PolyCounts.h"
#include "GU/GU_Detail.h"
#include "GU/GU_PrimPoly.h"
#include "UT/UT_Vector3.h"

#ifdef P4H_TC_GCC
#	pragma GCC diagnostic pop
#endif

#include <string>
#include <vector>


namespace p4h {

class HoudiniGeometry : public HoudiniCallbacks {
public:
	HoudiniGeometry(GU_Detail* gdp, prt::AttributeMapBuilder* eab = nullptr);

protected:
	virtual void setVertices(const double* vtx, size_t size);
	virtual void setNormals(const double* nrm, size_t size);
	virtual void setUVs(const double* uvs, size_t size);
	virtual void setFaces(
		int32_t* counts, size_t countsSize,
		int32_t* connects, size_t connectsSize,
		uint32_t* uvCounts, size_t uvCountsSize,
		uint32_t* uvConnects, size_t uvConnectsSize,
		int32_t* holes, size_t holesSize
	);
	virtual void setMaterials(
			const uint32_t* faceRanges,
			const prt::AttributeMap** materials,
			size_t size
	);
	virtual void createMesh(const wchar_t* name);
	virtual void finishMesh();

	virtual prt::Status generateError(size_t isIndex, prt::Status status, const wchar_t* message);
	virtual prt::Status assetError(size_t isIndex, prt::CGAErrorLevel level, const wchar_t* key, const wchar_t* uri, const wchar_t* message);
	virtual prt::Status cgaError(size_t isIndex, int32_t shapeID, prt::CGAErrorLevel level, int32_t methodId, int32_t pc, const wchar_t* message);
	virtual prt::Status cgaPrint(size_t isIndex, int32_t shapeID, const wchar_t* txt);
	virtual prt::Status cgaReportBool(size_t isIndex, int32_t shapeID, const wchar_t* key, bool value);
	virtual prt::Status cgaReportFloat(size_t isIndex, int32_t shapeID, const wchar_t* key, double value);
	virtual prt::Status cgaReportString(size_t isIndex, int32_t shapeID, const wchar_t* key, const wchar_t* value);
	virtual prt::Status attrBool(size_t isIndex, int32_t shapeID, const wchar_t* key, bool value);
	virtual prt::Status attrFloat(size_t isIndex, int32_t shapeID, const wchar_t* key, double value);
	virtual prt::Status attrString(size_t isIndex, int32_t shapeID, const wchar_t* key, const wchar_t* value);

private:
	GU_Detail* mDetail;
	GA_Offset mCurrentPrimitiveStartOffset;
	GA_PrimitiveGroup* mCurGroup;
	std::vector<UT_Vector3> mPoints, mNormals, mUVs;
	std::vector<int32_t> mIndices, mHoles;
	GEO_PolyCounts mPolyCounts;
	std::vector<uint32_t> mUVCounts, mUVIndices;

	prt::AttributeMapBuilder* const mEvalAttrBuilder;
};

} // namespace p4h
