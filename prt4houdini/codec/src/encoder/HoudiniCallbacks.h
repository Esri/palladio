#ifndef HOUDINICALLBACKS_H_
#define HOUDINICALLBACKS_H_

#include "prt/Callbacks.h"


class HoudiniCallbacks : public prt::Callbacks {
public:
	virtual ~HoudiniCallbacks() { }

	virtual void setVertices(double* vtx, size_t size) = 0;
	virtual void setNormals(double* nrm, size_t size) = 0;
	virtual void setUVs(float* u, float* v, size_t size) = 0;

	virtual void setFaces(int* counts, size_t countsSize, int* connects, size_t connectsSize, int* uvCounts, size_t uvCountsSize, int* uvConnects, size_t uvConnectsSize) = 0;
	virtual void createMesh(const wchar_t* name) = 0;
	virtual void finishMesh() = 0;

	virtual void matSetColor(int start, int count, float r, float g, float b) = 0;
	virtual void matSetDiffuseTexture(int start, int count, const wchar_t* tex) = 0;
};

#endif /* HOUDINICALLBACKS_H_ */

