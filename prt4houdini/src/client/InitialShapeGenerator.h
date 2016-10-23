#pragma once

#include "client/PRTContext.h"
#include "client/InitialShapeContext.h"

#include "GU/GU_Detail.h"


namespace p4h {

class InitialShapeGenerator final {
public:
	InitialShapeGenerator(const PRTContextUPtr& prtCtx, const GU_Detail* detail);
	InitialShapeGenerator(const InitialShapeGenerator&) = delete;
	InitialShapeGenerator& operator=(const InitialShapeGenerator&) = delete;
	virtual ~InitialShapeGenerator();

	InitialShapeNOPtrVector& getInitialShapes() { return mIS; } // TODO: fix const

private:
	void createInitialShapes(const PRTContextUPtr& prtCtx, const GU_Detail* detail);

	InitialShapeBuilderPtr	mISB;
	InitialShapeNOPtrVector mIS;
	AttributeMapNOPtrVector mISAttrs;
};

} // namespace p4h
