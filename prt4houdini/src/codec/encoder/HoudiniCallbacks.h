#pragma once

#include "prt/Callbacks.h"


constexpr uint32_t UV_IDX_NO_VALUE = -1;

class HoudiniCallbacks : public prt::Callbacks {
public:

	virtual ~HoudiniCallbacks() override = default;

	/**
	 * @param materials pre-condition: all materials must have an identical set of keys and types
	 * @param faceRanges contains materialsSize+1 values
	 */
	virtual void add(
			const wchar_t* name,
			const double* vtx, size_t vtxSize,
			const double* nrm, size_t nrmSize,
			const double* uvs, size_t uvsSize,
			int32_t* counts, size_t countsSize,
			int32_t* indices, size_t indicesSize,
			uint32_t* uvIndices, size_t uvIndicesSize,
			int32_t* holes, size_t holesSize,
			const prt::AttributeMap** materials, size_t materialsSize,
			const uint32_t* faceRanges
	) = 0;
};
