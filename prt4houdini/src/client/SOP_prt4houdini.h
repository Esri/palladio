#pragma once

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

#include <memory>


namespace prt {
class CacheObject;
class AttributeMap;
class AttributeMapBuilder;
class ResolveMap;
}


namespace prt4hdn {

class SOP_PRT : public SOP_Node {
public:
	typedef std::map<prt::Attributable::PrimitiveType,std::vector<std::string>> TypedParamNames;

public:
	static OP_Node* create(OP_Network*, const char*, OP_Operator*);

public:
	SOP_PRT(OP_Network *net, const char *name, OP_Operator *op);
	virtual ~SOP_PRT();

protected:
	virtual OP_ERROR cookMySop(OP_Context &context);
	virtual bool updateParmsFlags();

private:
	void createInitialShape(const GA_Group* group, void* ctx);
	bool handleParams(OP_Context &context);

private:
	std::unique_ptr<log::LogHandler>	mLogHandler;
	prt::CacheObject* mPRTCache;
	const prt::ResolveMap* mAssetsMap;

	std::wstring	mRPKURI;
	std::wstring	mRuleFile;
	std::wstring	mStyle;
	std::wstring	mStartRule;
	int32_t			mSeed;
	prt::AttributeMapBuilder* mAttributeSource;

	TypedParamNames mActiveParams;

	const prt::AttributeMap* mHoudiniEncoderOptions;;
	const prt::AttributeMap* mCGAPrintOptions;
	const prt::AttributeMap* mCGAErrorOptions;
	std::vector<const wchar_t*> mAllEncoders;
	std::vector<const prt::AttributeMap*> mAllEncoderOptions;
};

} // namespace prt4hdn
