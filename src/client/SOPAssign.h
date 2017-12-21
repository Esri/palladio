#pragma once

#include "PRTContext.h"
#include "ShapeConverter.h"

#include "SOP/SOP_Node.h"


class SOPAssign : public SOP_Node {
public:
	SOPAssign(const PRTContextUPtr& pCtx, OP_Network *net, const char *name, OP_Operator *op);
	~SOPAssign() override = default;

	const PRTContextUPtr& getPRTCtx() const { return mPRTCtx; }
	const ShapeConverterUPtr& getShapeConverter() const { return mShapeConverter; }

protected:
	OP_ERROR cookMySop(OP_Context &context) override;

private:
	void evaluatePrimitiveClassifier(OP_Context &context);
	void evaluateSharedShapeData(OP_Context &context);

	const PRTContextUPtr& mPRTCtx;
	PrimitiveClassifier   mPrimCls;
	ShapeConverterUPtr    mShapeConverter;
};
