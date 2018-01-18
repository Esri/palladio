#pragma once

#include "NodeParameter.h"

#include "SOP/SOP_Node.h"
#include "GA/GA_Primitive.h"
#include "GU/GU_Detail.h"

#include "boost/variant.hpp"

#include <vector>
#include <map>

class PrimitiveClassifier;

class PrimitivePartition {
public:
	using ClassifierValueType = boost::variant<UT_String, int32>;
	using PrimitiveVector     = std::vector<const GA_Primitive*>;
	using PartitionMap        = std::map<ClassifierValueType, PrimitiveVector>;

	PartitionMap mPrimitives;

	PrimitivePartition(const GA_Detail* detail, const PrimitiveClassifier& primCls);
	void add(const GA_Detail* detail, const PrimitiveClassifier& primCls, const GA_Primitive* p);

	const PartitionMap& get() const {
		return mPrimitives;
	}
};
