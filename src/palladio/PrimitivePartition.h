/*
 * Copyright 2014-2018 Esri R&D Zurich and VRBN
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#pragma once

#include "NodeParameter.h"

#include "SOP/SOP_Node.h"
#include "GA/GA_Primitive.h"
#include "GU/GU_Detail.h"

#include "BoostRedirect.h"
#include PLD_BOOST_INCLUDE(/variant.hpp)

#include <vector>
#include <map>


class PrimitiveClassifier;

class PrimitivePartition {
public:
	using ClassifierValueType = PLD_BOOST_NS::variant<UT_String, int32>;
	using PrimitiveVector     = std::vector<const GA_Primitive*>;
	using PartitionMap        = std::map<ClassifierValueType, PrimitiveVector>;

	PartitionMap mPrimitives;

	PrimitivePartition(const GA_Detail* detail, const PrimitiveClassifier& primCls);
	void add(const GA_Detail* detail, const PrimitiveClassifier& primCls, const GA_Primitive* p);

	const PartitionMap& get() const {
		return mPrimitives;
	}
};
