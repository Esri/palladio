#pragma once

#include "client/PRTContext.h"
#include "client/InitialShapeContext.h"
#include "client/logging.h"

#include "SOP/SOP_Node.h"


namespace prt {
class AttributeMap;
class AttributeMapBuilder;
} // namespace prt


namespace p4h {

class SOPAssign : public SOP_Node {
public:
	SOPAssign(const PRTContextUPtr& pCtx, OP_Network *net, const char *name, OP_Operator *op);
	virtual ~SOPAssign();

	const InitialShapeContext& getInitialShapeContext() const { return mInitialShapeContext; }
	void resetUserAttribute(const std::string& token);

protected:
	virtual OP_ERROR cookMySop(OP_Context &context);

private:
	void updateUserAttributes();
	bool handleParams(OP_Context &context);
	bool updateRulePackage(const boost::filesystem::path& nextRPK, fpreal time);
	void createMultiParams(fpreal time);

private:
	const PRTContextUPtr&		mPRTCtx;
	InitialShapeContext			mInitialShapeContext;

	log::LogHandlerPtr 			mLogHandler;
};

} // namespace p4h
