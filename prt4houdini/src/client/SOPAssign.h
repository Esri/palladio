#pragma once

#include "PRTContext.h"
#include "InitialShapeContext.h"
#include "logging.h"

#include "SOP/SOP_Node.h"


namespace prt {
class AttributeMap;
class AttributeMapBuilder;
} // namespace prt


namespace p4h {

class SOPAssign : public SOP_Node {
public:
	SOPAssign(const PRTContextUPtr& pCtx, OP_Network *net, const char *name, OP_Operator *op);
	~SOPAssign() override;

	const InitialShapeContext& getInitialShapeContext() const { return mInitialShapeContext; }
	const PRTContextUPtr& getPRTCtx() const { return mPRTCtx; }

protected:
	OP_ERROR cookMySop(OP_Context &context) override;

private:
	bool handleParams(OP_Context &context);
	bool updateRulePackage(const boost::filesystem::path& nextRPK, fpreal time);

private:
	log::LogHandlerPtr    mLogHandler;
	const PRTContextUPtr& mPRTCtx;
	InitialShapeContext   mInitialShapeContext;
};

void getDefaultRuleAttributeValues(
		AttributeMapBuilderPtr& amb,
		CacheObjectPtr& cache,
		const ResolveMapUPtr& resolveMap,
		const std::wstring& cgbKey,
		const std::wstring& startRule
);

} // namespace p4h
