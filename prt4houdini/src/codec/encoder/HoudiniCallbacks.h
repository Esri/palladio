#pragma once

#include "prt/Callbacks.h"


class HoudiniCallbacks : public prt::Callbacks {
public:
	virtual ~HoudiniCallbacks() { }

	// TODO: combine all calls into 'createModel'
	virtual void setVertices(const double* vtx, size_t size) = 0;
	virtual void setNormals(const double* nrm, size_t size) = 0;
	virtual void setUVs(const double* uvs, size_t size) = 0;
	virtual void setFaces(
		int32_t* counts, size_t countsSize,
		int32_t* indices, size_t indicesSize,
		uint32_t* uvCounts, size_t uvCountsSize,
		uint32_t* uvIndices, size_t uvIndicesSize,
		int32_t* holes, size_t holesSize
	) = 0;

	/**
	 * ranges entry ranges[i] points to the materials[i]
	 * i'th material goes from face ranges[i-1] to ranges[i]
	 */
	virtual void setMaterials(
		const uint32_t* faceRanges,
		const prt::AttributeMap** materials,
		size_t size
	) = 0;

	virtual void createMesh(const wchar_t* name) = 0;
	virtual void finishMesh() = 0;
};
