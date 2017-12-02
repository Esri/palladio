#pragma once

#include "PRTContext.h"
#include "ShapeConverter.h"
#include "logging.h"
#include "utils.h"

#include "SOP/SOP_Node.h"


class SOPGenerate : public SOP_Node {
public:
	SOPGenerate(const PRTContextUPtr& pCtx, OP_Network *net, const char *name, OP_Operator *op);
	~SOPGenerate() override = default;

protected:
	OP_ERROR cookMySop(OP_Context &context) override;

private:
	bool handleParams(OP_Context& context);

private:
	const PRTContextUPtr&       mPRTCtx;

	AttributeMapUPtr            mHoudiniEncoderOptions;
	AttributeMapUPtr            mCGAPrintOptions;
	AttributeMapUPtr            mCGAErrorOptions;
	std::vector<const wchar_t*> mAllEncoders;
	AttributeMapNOPtrVector     mAllEncoderOptions;
	AttributeMapUPtr            mGenerateOptions;
};
