/*
 * Copyright 2014-2020 Esri R&D Zurich and VRBN
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
#include "PrimitivePartition.h"
#include "Utils.h"

#include "GA/GA_Primitive.h"

using PrimitiveNOPtrVector = std::vector<const GA_Primitive*>;

class ShapeData final {
public:
	ShapeData() = default;
	ShapeData(const GroupCreation& gc, const std::wstring& namePrefix) : mGroupCreation(gc), mNamePrefix(namePrefix) {}
	ShapeData(const ShapeData&) = delete;
	ShapeData(ShapeData&&) = delete;
	~ShapeData();

	void addBuilder(InitialShapeBuilderUPtr&& isb, int32_t randomSeed, const PrimitiveNOPtrVector& primMappings,
	                const PrimitivePartition::ClassifierValueType& clsVal);

	void addShape(const prt::InitialShape* is, AttributeMapBuilderUPtr&& amb, AttributeMapUPtr&& ruleAttr);

	InitialShapeBuilderVector& getInitialShapeBuilders() {
		return mInitialShapeBuilders;
	}
	int32_t getInitialShapeRandomSeed(size_t isIdx) const {
		return mRandomSeeds[isIdx];
	}
	InitialShapeBuilderUPtr& getInitialShapeBuilder(size_t isIdx) {
		return mInitialShapeBuilders[isIdx];
	}
	const PrimitiveNOPtrVector& getPrimitiveMapping(size_t isIdx) const {
		return mPrimitiveMapping[isIdx];
	}

	AttributeMapBuilderVector& getRuleAttributeMapBuilders() {
		return mRuleAttributeBuilders;
	}
	const AttributeMapBuilderVector& getRuleAttributeMapBuilders() const {
		return mRuleAttributeBuilders;
	}

	const std::wstring& getInitialShapeName(size_t isIdx) const;
	const InitialShapeNOPtrVector& getInitialShapes() const {
		return mInitialShapes;
	}

	bool isValid() const;

private:
	std::vector<PrimitiveNOPtrVector> mPrimitiveMapping;

	InitialShapeBuilderVector mInitialShapeBuilders;
	InitialShapeNOPtrVector mInitialShapes;

	AttributeMapBuilderVector mRuleAttributeBuilders;
	AttributeMapVector mRuleAttributes;

	GroupCreation mGroupCreation = GroupCreation::NONE;
	std::vector<std::wstring> mInitialShapeNames;
	std::wstring mNamePrefix;

	std::vector<int32_t> mRandomSeeds;
};
