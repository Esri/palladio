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

struct InitialShapeContext {
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
