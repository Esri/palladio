#pragma once

#include "client/utils.h"
#include "client/logging.h"

#include "prt/API.h"

#ifdef P4H_TC_GCC
#	pragma GCC diagnostic push
#	pragma GCC diagnostic ignored "-Wunused-local-typedefs"
#endif

#include "GU/GU_Detail.h"
#include "UT/UT_String.h"

#ifdef P4H_TC_GCC
#	pragma GCC diagnostic pop
#endif

#include "boost/filesystem/path.hpp"

#include <string>
#include <map>


namespace p4h {

typedef std::vector<const prt::InitialShape*> InitialShapeNOPtrVector;
typedef std::vector<const prt::AttributeMap*> AttributeMapNOPtrVector;

typedef std::unique_ptr<const prt::AttributeMap, utils::PRTDestroyer> AttributeMapPtr;
typedef std::unique_ptr<prt::AttributeMapBuilder, utils::PRTDestroyer> AttributeMapBuilderPtr;
typedef std::unique_ptr<const prt::InitialShape, utils::PRTDestroyer> InitialShapePtr;
typedef std::unique_ptr<prt::InitialShapeBuilder, utils::PRTDestroyer> InitialShapeBuilderPtr;
typedef std::unique_ptr<const prt::ResolveMap, utils::PRTDestroyer> ResolveMapPtr;
typedef std::unique_ptr<const prt::RuleFileInfo, utils::PRTDestroyer> RuleFileInfoPtr;
typedef std::unique_ptr<const prt::EncoderInfo, utils::PRTDestroyer> EncoderInfoPtr;

struct InitialShapeContext {
	InitialShapeContext() { }

	UT_String				mShapeClsAttrName;
	GA_StorageClass			mShapeClsType;
	boost::filesystem::path	mRPK;
	std::wstring			mRuleFile;
	std::wstring			mStyle;
	std::wstring			mStartRule;
	int32_t					mSeed;

	ResolveMapPtr			mAssetsMap;

	AttributeMapPtr			mRuleAttributeValues; // rule attribute values as defined in the rule file
	AttributeMapPtr			mUserAttributeValues; // holds rule or user attribute values for next generate run
};

class InitialShapeGenerator {
public:
	InitialShapeGenerator(GU_Detail* gdp, InitialShapeContext& isCtx);
	virtual ~InitialShapeGenerator();

	InitialShapeNOPtrVector& getInitialShapes() { return mInitialShapes; } // TODO: fix const

private:
	void createInitialShapes(GU_Detail* gdp,InitialShapeContext& isCtx);

	InitialShapeBuilderPtr	mISB;
	InitialShapeNOPtrVector mInitialShapes;
	AttributeMapNOPtrVector mInitialShapeAttributes;
};

} // namespace p4h
