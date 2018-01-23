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

#include "prt/Callbacks.h"

constexpr const wchar_t* ENCODER_ID_HOUDINI = L"HoudiniEncoder";
constexpr const wchar_t* EO_EMIT_ATTRIBUTES = L"emitAttributes";
constexpr const wchar_t* EO_EMIT_MATERIALS  = L"emitMaterials";
constexpr const wchar_t* EO_EMIT_REPORTS    = L"emitReports";


class HoudiniCallbacks : public prt::Callbacks {
public:

	virtual ~HoudiniCallbacks() override = default;

	/**
	 * @param uvCounts uv index count per face in groups of "uvSets" (f0 uvset 0, f0 uvset 1, ..., f1 uvset 0, ...)
	 * @param uvIndices all uv indices of all uv sets
	 * @param uvSets number of uv sets
	 * @param faceRanges ranges for materials and reports
	 * @param materials contains faceRangesSize-1 attribute maps (all materials must have an identical set of keys and types)
	 * @param reports contains faceRangesSize-1 attribute maps
	 * @param shapeIDs shape ids per face, contains faceRangesSize-1 values
	 */
	virtual void add(
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
	) = 0;
};
