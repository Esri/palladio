#pragma once

#include "PRTContext.h"
#include "ShapeData.h"
#include "utils.h"


class GU_Detail;

namespace p4h {

class InitialShapeGenerator final {
public:
	InitialShapeGenerator(const PRTContextUPtr& prtCtx, const GU_Detail* detail);
	InitialShapeGenerator(const InitialShapeGenerator&) = delete;
	InitialShapeGenerator(InitialShapeGenerator&&) = delete;
	InitialShapeGenerator& operator=(const InitialShapeGenerator&) = delete;
	InitialShapeGenerator&& operator=(InitialShapeGenerator&&) = delete;
	~InitialShapeGenerator();

	const InitialShapeNOPtrVector& getInitialShapes() const { return mIS; }

private:
	void createInitialShapes(const PRTContextUPtr& prtCtx, const GU_Detail* detail);

	ShapeData shapeData;
	InitialShapeNOPtrVector mIS;
};

} // namespace p4h
