#pragma once

#include "client/logging.h"
#include "client/utils.h"
#include "codec/encoder/HoudiniCallbacks.h"

#include "prt/AttributeMap.h"
#include "prt/Callbacks.h"

#include "GEO/GEO_PolyCounts.h"
#include "GU/GU_Detail.h"
#include "GU/GU_PrimPoly.h"
#include "UT/UT_Vector3.h"

#include <string>
#include <vector>


namespace p4h {

class HoudiniGeometry : public HoudiniCallbacks {
public:
	HoudiniGeometry(GU_Detail* gdp, prt::AttributeMapBuilder* eab = nullptr);

protected:
	virtual void setVertices(double* vtx, size_t size);
	virtual void setNormals(double* nrm, size_t size);
	virtual void setUVs(float* u, float* v, size_t size);
	virtual void setFaces(int* counts, size_t countsSize, int* connects, size_t connectsSize, int* uvCounts, size_t uvCountsSize, int* uvConnects, size_t uvConnectsSize);
	virtual void createMesh(const wchar_t* name);
	virtual void matSetColor(int start, int count, float r, float g, float b);
	virtual void matSetDiffuseTexture(int start, int count, const wchar_t* tex);
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
	GA_Offset mCurOffset;
	GA_PrimitiveGroup* mCurGroup;
	std::vector<UT_Vector3> mPoints;
	std::vector<int> mIndices;
	GEO_PolyCounts mPolyCounts;

	prt::AttributeMapBuilder* const mEvalAttrBuilder;
};

} // namespace p4h
