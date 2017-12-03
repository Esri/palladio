#pragma once

#include "GU/GU_Detail.h"
#include "GA/GA_Primitive.h"

#include "boost/variant.hpp"

#include <vector>
#include <map>


const UT_String CE_SHAPE_CLS_NAME   = "ceShapeClsName";
const UT_String CE_SHAPE_CLS_TYPE   = "ceShapeClsType"; // points to ordinal of GA_StorageClass

struct PrimitivePartition {
	using ClassifierValueType = boost::variant<UT_String, fpreal32, int32>;
	using PrimitiveVector     = std::vector<const GA_Primitive*>;
	using PartitionMap        = std::map<ClassifierValueType, PrimitiveVector>;

	PartitionMap mPrimitives;

	PrimitivePartition(const GU_Detail* detail, const UT_String& assignShpClsAttrName, const GA_StorageClass& assignShpClsAttrType);
	void add(const GA_Detail* detail, const GA_Primitive* p, const UT_String& assignShpClsAttrName, const GA_StorageClass& assignShpClsAttrType);

	const PartitionMap& get() const {
		return mPrimitives;
	}
};
