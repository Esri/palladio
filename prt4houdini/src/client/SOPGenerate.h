#pragma once

#include "client/PRTContext.h"
#include "client/InitialShapeContext.h"

#include "SOP/SOP_Node.h"


namespace p4h {

class SOPGenerate : public SOP_Node {
public:
	SOPGenerate(const PRTContextUPtr& pCtx, OP_Network *net, const char *name, OP_Operator *op);
	virtual ~SOPGenerate();

protected:
	virtual OP_ERROR cookMySop(OP_Context &context);

private:
	const PRTContextUPtr&		mPRTCtx;

	AttributeMapPtr				mHoudiniEncoderOptions;
	AttributeMapPtr				mCGAPrintOptions;
	AttributeMapPtr				mCGAErrorOptions;
	std::vector<const wchar_t*> mAllEncoders;
	AttributeMapNOPtrVector 	mAllEncoderOptions;
	AttributeMapPtr				mGenerateOptions;
};

} // namespace p4h
