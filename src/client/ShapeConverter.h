#pragma once

#include "PRTContext.h"
#include "utils.h"

#include "boost/filesystem/path.hpp"

#include <string>
#include <vector>
#include <memory>


class GU_Detail;

struct ShapeData final {
	std::vector<PrimitiveNOPtrVector> mPrimitiveMapping;

	InitialShapeBuilderVector         mInitialShapeBuilders;
	InitialShapeNOPtrVector           mInitialShapes;

	AttributeMapBuilderVector         mRuleAttributeBuilders;
	AttributeMapVector                mRuleAttributes;

	~ShapeData() {
		std::for_each(mInitialShapes.begin(), mInitialShapes.end(), [](const prt::InitialShape* is) { is->destroy(); });
	}
};


struct ShapeConverter {
    virtual void get(const GU_Detail* detail, ShapeData& shapeData, const PRTContextUPtr& prtCtx);
	void put(GU_Detail* detail, const ShapeData& shapeData) const;

	std::wstring getFullyQualifiedStartRule() const { return mStyle + L'$' + mStartRule; }

	UT_String               mShapeClsAttrName;
	GA_StorageClass	        mShapeClsType;

	boost::filesystem::path mRPK;
	std::wstring            mRuleFile;
	std::wstring            mStyle;
	std::wstring            mStartRule;
	int32_t                 mSeed = 0;

	RuleFileInfoUPtr        mRuleFileInfo;
};

using ShapeConverterUPtr = std::unique_ptr<ShapeConverter>;
