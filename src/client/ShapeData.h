#pragma once

#include "PRTContext.h"
#include "utils.h"

#include "GA/GA_Types.h"
#include "GA/GA_Primitive.h"
#include "GA/GA_AttributeRef.h"
#include "UT/UT_String.h"

#include "boost/filesystem/path.hpp"

#include <string>
#include <vector>
#include <memory>


class GU_Detail;

namespace p4h {

struct ShapeData final {
	using PrimitiveVector = std::vector<const GA_Primitive*>;

	std::vector<PrimitiveVector> mPrimitiveMapping;

	InitialShapeBuilderVector    mInitialShapeBuilders;

	AttributeMapBuilderVector    mRuleAttributeBuilders;
	AttributeMapVector           mRuleAttributes; // TODO: switch to shared ptr

	// TODO: also carry resolvemap shared ptrs
};

class SharedShapeData;
using SharedShapeDataUPtr = std::unique_ptr<SharedShapeData>;

struct SharedShapeData final {
	SharedShapeData();
	SharedShapeData(const SharedShapeData&) = delete;
	SharedShapeData(SharedShapeData&&) = delete;
	SharedShapeData& operator=(const SharedShapeData&) = delete;
	SharedShapeData&& operator=(SharedShapeData&&) = delete;
	~SharedShapeData() = default;

    void get(const GU_Detail* detail, ShapeData& shapeData, const PRTContextUPtr& prtCtx);
	void put(GU_Detail* detail, ShapeData& shapeData); // TODO: make shapedata and put itself const

	std::wstring getFullyQualifiedStartRule() const { return mStyle + L'$' + mStartRule; }

	UT_String               mShapeClsAttrName;
	GA_StorageClass	        mShapeClsType;

	boost::filesystem::path mRPK;
	std::wstring            mRuleFile;
	std::wstring            mStyle;
	std::wstring            mStartRule;
	int32_t                 mSeed;

	RuleFileInfoUPtr        mRuleFileInfo;
};

} // namespace p4h
