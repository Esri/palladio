#pragma once

#include "NodeParameter.h"

#include "GA/GA_Primitive.h"
#include "GU/GU_Detail.h"

#include "boost/variant.hpp"

#include <vector>
#include <map>


const UT_String CE_PRIM_CLS_NAME = "cePrimClsName";
const UT_String CE_PRIM_CLS_TYPE = "cePrimClsType";

struct PrimitiveClassifier {
	UT_String       name;
	GA_StorageClass type = AssignNodeParams::DEFAULT_PRIM_CLS_TYPE_VALUE;

	PrimitiveClassifier updateFromPrimitive(const GA_Detail* detail, const GA_Primitive* p) const;
};


class PrimitivePartition {
public:
	using ClassifierValueType = boost::variant<UT_String, fpreal32, int32>;
	using PrimitiveVector     = std::vector<const GA_Primitive*>;
	using PartitionMap        = std::map<ClassifierValueType, PrimitiveVector>;

	PartitionMap mPrimitives;

	PrimitivePartition(const GA_Detail* detail, const PrimitiveClassifier& primCls);
	void add(const GA_Detail* detail, const PrimitiveClassifier& primCls, const GA_Primitive* p);

	const PartitionMap& get() const {
		return mPrimitives;
	}
};
