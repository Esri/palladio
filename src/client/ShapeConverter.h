#pragma once

#include "PRTContext.h"
#include "utils.h"

#include "boost/filesystem/path.hpp"

#include <string>
#include <vector>
#include <memory>


class GU_Detail;

class ShapeData final {
public:
	std::vector<PrimitiveNOPtrVector> mPrimitiveMapping;

	InitialShapeBuilderVector         mInitialShapeBuilders;
	InitialShapeNOPtrVector           mInitialShapes;

	AttributeMapBuilderVector         mRuleAttributeBuilders;
	AttributeMapVector                mRuleAttributes;

	bool isValid() const {
		const size_t numISB = mInitialShapeBuilders.size();
		const size_t numIS = mInitialShapes.size();

		const size_t numAMB = mRuleAttributeBuilders.size();
		const size_t numAM = mRuleAttributes.size();

		const size_t numPM = mPrimitiveMapping.size();

		if (numISB != numPM)
			return false;

		if (numIS != numAMB || numIS != numAM) // they are allowed to be all 0
			return false;

		return true;
	}

	~ShapeData() {
		std::for_each(mInitialShapes.begin(), mInitialShapes.end(), [](const prt::InitialShape* is){
			if (is)
				is->destroy();
		});
	}
};


class ShapeConverter {
public:
	virtual void get(const GU_Detail* detail, ShapeData& shapeData, const PRTContextUPtr& prtCtx);
	void put(GU_Detail* detail, const ShapeData& shapeData) const;

	RuleFileInfoUPtr getRuleFileInfo(const ResolveMapUPtr& resolveMap, prt::Cache* prtCache) const;
	std::wstring getFullyQualifiedStartRule() const;

public:
	UT_String               mShapeClsAttrName;
	GA_StorageClass	        mShapeClsType;

	boost::filesystem::path mRPK;
	std::wstring            mRuleFile;
	std::wstring            mStyle;
	std::wstring            mStartRule;

	int64_t                 mSeed = 0;
};

using ShapeConverterUPtr = std::unique_ptr<ShapeConverter>;


namespace NameConversion {

UT_String toPrimAttr(const std::string& name);
std::string toRuleAttr(const UT_StringHolder& name);

} // namespace NameConversion