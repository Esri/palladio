#pragma once

#include "client/InitialShapeContext.h"

#include "GU/GU_Detail.h"


namespace p4h {

class InitialShapeGenerator final {
public:
	InitialShapeGenerator() = delete;
	InitialShapeGenerator(const InitialShapeGenerator&) = delete;
	InitialShapeGenerator& operator=(const InitialShapeGenerator&) = delete;
	InitialShapeGenerator(GU_Detail* gdp, InitialShapeContext& isCtx);
	virtual ~InitialShapeGenerator();

	InitialShapeNOPtrVector& getInitialShapes() { return mInitialShapes; } // TODO: fix const

private:
	void createInitialShapes(GU_Detail* gdp,InitialShapeContext& isCtx);

	InitialShapeBuilderPtr	mISB;
	InitialShapeNOPtrVector mInitialShapes;
	AttributeMapNOPtrVector mInitialShapeAttributes;
};

} // namespace p4h
