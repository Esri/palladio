#pragma once

#include "utils.h"
#include "../codec/encoder/HoudiniCallbacks.h"

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
#include "UT/UT_Interrupt.h"

#ifdef P4H_TC_GCC
#	pragma GCC diagnostic pop
#endif

#include <string>
#include <vector>
#include <mutex>


namespace ModelConversion {

void getUVSet(
	std::vector<uint32_t>& uvIndicesPerSet,
	const uint32_t* counts, size_t countsSize,
	const uint32_t* uvCounts, size_t uvCountsSize,
	uint32_t uvSet, uint32_t uvSets,
	const uint32_t* uvIndices, size_t uvIndicesSize
);

void setUVs(
	GA_RWHandleV3& handle, const GA_Detail::OffsetMarker& marker,
	const uint32_t* counts, size_t countsSize,
	const uint32_t* uvCounts, size_t uvCountsSize,
	uint32_t uvSet, uint32_t uvSets,
	const uint32_t* uvIndices, size_t uvIndicesSize,
	const double* uvs, size_t uvsSize
);

} // namespace ModelConversion


class ModelConverter : public HoudiniCallbacks {
public:
	explicit ModelConverter(GU_Detail* gdp, UT_AutoInterrupt* autoInterrupt = nullptr);

protected:
	void add(
			const wchar_t* name,
			const double* vtx, size_t vtxSize,
			const double* nrm, size_t nrmSize,
			const double* uvs, size_t uvsSize,
			const uint32_t* counts, size_t countsSize,
			const uint32_t* indices, size_t indicesSize,
			const uint32_t* uvCounts, size_t uvCountsSize,
			const uint32_t* uvIndices, size_t uvIndicesSize,
			uint32_t uvSets,
			const prt::AttributeMap** materials, size_t materialsSize,
			const uint32_t* faceRanges
	) override;

	prt::Status generateError(size_t isIndex, prt::Status status, const wchar_t* message) override;
	prt::Status assetError(size_t isIndex, prt::CGAErrorLevel level, const wchar_t* key, const wchar_t* uri, const wchar_t* message) override;
	prt::Status cgaError(size_t isIndex, int32_t shapeID, prt::CGAErrorLevel level, int32_t methodId, int32_t pc, const wchar_t* message) override;
	prt::Status cgaPrint(size_t isIndex, int32_t shapeID, const wchar_t* txt) override;
	prt::Status cgaReportBool(size_t isIndex, int32_t shapeID, const wchar_t* key, bool value) override;
	prt::Status cgaReportFloat(size_t isIndex, int32_t shapeID, const wchar_t* key, double value) override;
	prt::Status cgaReportString(size_t isIndex, int32_t shapeID, const wchar_t* key, const wchar_t* value) override;
	prt::Status attrBool(size_t isIndex, int32_t shapeID, const wchar_t* key, bool value) override;
	prt::Status attrFloat(size_t isIndex, int32_t shapeID, const wchar_t* key, double value) override;
	prt::Status attrString(size_t isIndex, int32_t shapeID, const wchar_t* key, const wchar_t* value) override;

	prt::Callbacks::Continuation progress(float percentageCompleted) override {
		if (mAutoInterrupt && mAutoInterrupt->wasInterrupted()) // TODO: is this thread-safe?
			return prt::Callbacks::CANCEL_AND_FINISH;
		return prt::Callbacks::progress(percentageCompleted);
	}

private:
	GU_Detail* mDetail;
	UT_AutoInterrupt* mAutoInterrupt;
};
