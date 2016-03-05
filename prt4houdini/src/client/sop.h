#pragma once

#include "client/initialshape.h"
#include "client/logging.h"

#ifdef P4H_TC_GCC
#	pragma GCC diagnostic push
#	pragma GCC diagnostic ignored "-Wunused-local-typedefs"
#	pragma GCC diagnostic ignored "-Wattributes"
#endif

#include "SOP/SOP_Node.h"

#ifdef P4H_TC_GCC
#	pragma GCC diagnostic pop
#endif

#include "prt/Attributable.h"
#include "prt/ResolveMap.h"
#include "prt/RuleFileInfo.h"

#include <memory>


namespace prt {
class CacheObject;
class AttributeMap;
class AttributeMapBuilder;
}


namespace p4h {

class SOP_PRT : public SOP_Node {
public:
	static OP_Node* create(OP_Network*, const char*, OP_Operator*);

public:
	SOP_PRT(OP_Network *net, const char *name, OP_Operator *op);
	virtual ~SOP_PRT();

	const InitialShapeContext& getInitialShapeCtx() const { return mInitialShapeContext; }
	void resetUserAttribute(const std::string& token);

protected:
	virtual OP_ERROR cookMySop(OP_Context &context);

private:
	void updateUserAttributes();
	bool handleParams(OP_Context &context);
	bool updateRulePackage(const boost::filesystem::path& nextRPK, fpreal time);
	void createMultiParams(fpreal time);

private:
	InitialShapeContext			mInitialShapeContext;

	CacheObjectPtr 				mPRTCache;
	AttributeMapPtr				mHoudiniEncoderOptions;
	AttributeMapPtr				mCGAPrintOptions;
	AttributeMapPtr				mCGAErrorOptions;
	std::vector<const wchar_t*> mAllEncoders;
	AttributeMapNOPtrVector 	mAllEncoderOptions;
	AttributeMapPtr				mGenerateOptions;

	log::LogHandlerPtr 			mLogHandler;
};

} // namespace p4h
