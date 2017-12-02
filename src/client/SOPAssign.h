#pragma once

#include "PRTContext.h"
#include "ShapeConverter.h"

#include "SOP/SOP_Node.h"


class SOPAssign : public SOP_Node {
public:
	SOPAssign(const PRTContextUPtr& pCtx, OP_Network *net, const char *name, OP_Operator *op);
	virtual ~SOPAssign() override = default;

	const PRTContextUPtr& getPRTCtx() const { return mPRTCtx; }
	const ShapeConverterUPtr& getSharedShapeData() const { return mSharedShapeData; }

protected:
	OP_ERROR cookMySop(OP_Context &context) override;

private:
	bool updateSharedShapeDataFromParams(OP_Context &context);
	bool updateRulePackage(const boost::filesystem::path& nextRPK, fpreal time);

private:
	const PRTContextUPtr& mPRTCtx;
	ShapeConverterUPtr   mSharedShapeData;
};
