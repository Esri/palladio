#pragma once

#include "client/shapegen.h"
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

#include "boost/filesystem.hpp"
#include "shapegen.h"

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
	static void buildStartRuleMenu(void* data, PRM_Name* theMenu, int theMaxSize, const PRM_SpareData*, const PRM_Parm*);
	static void buildRuleFileMenu(void* data, PRM_Name* theMenu, int theMaxSize, const PRM_SpareData*, const PRM_Parm*);

public:
	SOP_PRT(OP_Network *net, const char *name, OP_Operator *op);
	virtual ~SOP_PRT();

protected:
	virtual OP_ERROR cookMySop(OP_Context &context);
	virtual bool updateParmsFlags();

private:
	bool handleParams(OP_Context &context);
	bool updateRulePackage(const boost::filesystem::path& nextRPK, fpreal time);
	void createSpareParams(const RuleFileInfoPtr& info, const std::wstring& cgbKey, const std::wstring& fqStartRule, fpreal time);
	void getParamDef(const RuleFileInfoPtr& info, TypedParamNames& createdParams, std::ostream& defStream);
private:
	InitialShapeContext     mInitialShapeContext;

	prt::CacheObject* mPRTCache;
	const prt::AttributeMap* mHoudiniEncoderOptions;
	const prt::AttributeMap* mCGAPrintOptions;
	const prt::AttributeMap* mCGAErrorOptions;
	std::vector<const wchar_t*> mAllEncoders;
	std::vector<const prt::AttributeMap*> mAllEncoderOptions;

	std::unique_ptr<log::LogHandler> mLogHandler;
};

} // namespace p4h
