#pragma once

#include "prt/Callbacks.h"


class HoudiniCallbacks : public prt::Callbacks {
public:

	virtual ~HoudiniCallbacks() override = default;

	/**
	 * @param uvCounts uv index count per face in groups of "uvSets" (f0 uvset 0, f0 uvset 1, ..., f1 uvset 0, ...)
	 * @param uvIndices all uv indices of all uv sets
	 * @param uvSets number of uv sets
	 * @param materials pre-condition: all materials must have an identical set of keys and types
	 * @param faceRanges contains materialsSize+1 values
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
			const prt::AttributeMap** materials, size_t materialsSize,
			const uint32_t* faceRanges
	) = 0;
};
