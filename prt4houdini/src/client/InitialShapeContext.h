#pragma once

#include "client/utils.h"

#include "GA/GA_Types.h"
#include "UT/UT_String.h"

#include "boost/filesystem/path.hpp"

#include <string>


class GU_Detail;

namespace p4h {

class InitialShapeContext final {
public:
	InitialShapeContext() = default;
	InitialShapeContext(const InitialShapeContext&) = delete;
	InitialShapeContext& operator=(const InitialShapeContext&) = delete;
	InitialShapeContext(GU_Detail* detail);
	virtual ~InitialShapeContext() = default;

	void get(GU_Detail* detail);
	void put(GU_Detail* detail);

	UT_String				mShapeClsAttrName;
	GA_StorageClass			mShapeClsType;

	boost::filesystem::path	mRPK;
	std::wstring			mRuleFile;
	std::wstring			mStyle;
	std::wstring			mStartRule;
	int32_t					mSeed;

	ResolveMapPtr			mAssetsMap; // TODO: really needed here?

	AttributeMapPtr			mRuleAttributeValues; // rule attribute values as defined in the rule file
	AttributeMapPtr			mUserAttributeValues; // holds rule or user attribute values for next generate run
};

} // namespace p4h
