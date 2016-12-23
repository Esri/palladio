#pragma once

#include "client/utils.h"

#include "GA/GA_Types.h"
#include "GA/GA_AttributeRef.h"
#include "UT/UT_String.h"

#include "boost/filesystem/path.hpp"

#include <string>
#include <vector>


class GU_Detail;

namespace p4h {

class InitialShapeContext;
using InitialShapeContextUPtr = std::unique_ptr<InitialShapeContext>;

class InitialShapeContext final {
public:
	InitialShapeContext();
	InitialShapeContext(GU_Detail* detail);
	InitialShapeContext(const InitialShapeContext&) = delete;
	InitialShapeContext& operator=(const InitialShapeContext&) = delete;
	virtual ~InitialShapeContext() = default;

	void put(GU_Detail* detail);

	static GA_ROAttributeRef getClsName(const GU_Detail* detail);
	static GA_ROAttributeRef getClsType(const GU_Detail* detail);

	UT_String				mShapeClsAttrName;
	GA_StorageClass			mShapeClsType;

	boost::filesystem::path	mRPK;
	std::wstring			mRuleFile;
	std::wstring			mStyle;
	std::wstring			mStartRule;
	int32_t					mSeed;

	AttributeMapPtr			mRuleAttributeValues; // rule attribute values as defined in the rule file
	//AttributeMapPtr			mUserAttributeValues; // holds rule or user attribute values for next generate run
};

} // namespace p4h
