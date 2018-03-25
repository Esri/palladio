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

#pragma once

#include "PalladioMain.h"
#include "ShapeConverter.h"
#include "Utils.h"
#include "encoder/HoudiniCallbacks.h"

#include "prt/AttributeMap.h"
#include "prt/Callbacks.h"

#ifdef PLD_TC_GCC
#	pragma GCC diagnostic push
#	pragma GCC diagnostic ignored "-Wunused-local-typedefs"
#endif

#include "GEO/GEO_PolyCounts.h"
#include "GU/GU_Detail.h"
#include "GU/GU_PrimPoly.h"
#include "UT/UT_Vector3.h"
#include "UT/UT_Interrupt.h"

#ifdef PLD_TC_GCC
#	pragma GCC diagnostic pop
#endif

#include <string>
#include <vector>


namespace ModelConversion {

PLD_TEST_EXPORTS_API void getUVSet(
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
	explicit ModelConverter(GU_Detail* gdp, GroupCreation gc, UT_AutoInterrupt* autoInterrupt = nullptr);

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
			const uint32_t* faceRanges, size_t faceRangesSize,
			const prt::AttributeMap** materials,
			const prt::AttributeMap** reports,
			const int32_t* shapeIDs
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
	GroupCreation mGroupCreation;
	UT_AutoInterrupt* mAutoInterrupt;
	std::map<int32_t, AttributeMapBuilderUPtr> mShapeAttributeBuilders;
};

using ModelConverterUPtr = std::unique_ptr<ModelConverter>;
