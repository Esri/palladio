#pragma once

#include "PRTContext.h"
#include "ShapeConverter.h"

#include "SOP/SOP_Node.h"


class SOPAssign : public SOP_Node {
public:
	SOPAssign(const PRTContextUPtr& pCtx, OP_Network *net, const char *name, OP_Operator *op);
	~SOPAssign() override = default;

	const PRTContextUPtr& getPRTCtx() const { return mPRTCtx; }

	void opChanged(OP_EventType reason, void* data = nullptr) override;

protected:
	OP_ERROR cookMySop(OP_Context& context) override;

private:
	const PRTContextUPtr& mPRTCtx;
	ShapeConverterUPtr    mShapeConverter;
};
