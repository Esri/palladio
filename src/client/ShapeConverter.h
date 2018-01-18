#pragma once

#include "PRTContext.h"
#include "utils.h"

#include "boost/filesystem/path.hpp"

#include <string>


class GU_Detail;
class GA_Primitive;
class OP_Context;
class SOP_Node;
class PrimitiveClassifier;
class MainAttributeHandles;
class ShapeData;

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
};

using ShapeConverterUPtr = std::unique_ptr<ShapeConverter>;

