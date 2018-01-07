#pragma once

#include "PrimitivePartition.h"
#include "PRTContext.h"
#include "utils.h"

#include "boost/filesystem/path.hpp"

#include <string>
#include <vector>
#include <memory>


class GU_Detail;
class GA_Primitive;
class OP_Context;
class PrimitiveClassifier;
class MainAttributeHandles;


class ShapeData final {
public:
	std::vector<PrimitiveNOPtrVector> mPrimitiveMapping;

	InitialShapeBuilderVector         mInitialShapeBuilders;
	InitialShapeNOPtrVector           mInitialShapes;

	AttributeMapBuilderVector         mRuleAttributeBuilders;
	AttributeMapVector                mRuleAttributes;

	GenerateNodeParams::GroupCreation mGroupCreation = GenerateNodeParams::GroupCreation::NONE;
	std::vector<std::wstring>         mInitialShapeNames;
	std::wstring                      mNamePrefix;

	bool isValid() const {
		const size_t numISB = mInitialShapeBuilders.size();
		const size_t numIS = mInitialShapes.size();
		const size_t numISN = mInitialShapeNames.size();

		const size_t numAMB = mRuleAttributeBuilders.size();
		const size_t numAM = mRuleAttributes.size();

		const size_t numPM = mPrimitiveMapping.size();

		if (numISB != numPM || numISB != numISN)
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

const UT_String CE_SHAPE_RPK        = "ceShapeRPK";
const UT_String CE_SHAPE_RULE_FILE  = "ceShapeRuleFile";
const UT_String CE_SHAPE_START_RULE = "ceShapeStartRule";
const UT_String CE_SHAPE_STYLE      = "ceShapeStyle";
const UT_String CE_SHAPE_SEED       = "ceShapeSeed";

class ShapeConverter {
public:
	virtual void get(const GU_Detail* detail,  const PrimitiveClassifier& primCls,
	                 ShapeData& shapeData, const PRTContextUPtr& prtCtx);
	void put(GU_Detail* detail, PrimitiveClassifier& primCls, const ShapeData& shapeData) const;

	void getMainAttributes(SOP_Node* node, const OP_Context& context);
	bool getMainAttributes(const GU_Detail* detail, const GA_Primitive* prim);
	void putMainAttributes(MainAttributeHandles& mah, const GA_Primitive* primitive) const;

	RuleFileInfoUPtr getRuleFileInfo(const ResolveMapUPtr& resolveMap, prt::Cache* prtCache) const;
	std::wstring getFullyQualifiedStartRule() const;

public:
	boost::filesystem::path mRPK;
	std::wstring            mRuleFile;
	std::wstring            mStyle;
	std::wstring            mStartRule;
	int64_t                 mSeed = 0;
};

using ShapeConverterUPtr = std::unique_ptr<ShapeConverter>;

