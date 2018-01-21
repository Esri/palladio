#pragma once

#include "PrimitivePartition.h"
#include "NodeParameter.h"
#include "Utils.h"


class ShapeData final {
public:
	ShapeData() = default;
	ShapeData(const GenerateNodeParams::GroupCreation& gc, const std::wstring& namePrefix) : mGroupCreation(gc), mNamePrefix(namePrefix) { }
	ShapeData(const ShapeData&) = delete;
	ShapeData(ShapeData&&) = delete;
	~ShapeData();

	void addBuilder(InitialShapeBuilderUPtr&& isb, int32_t randomSeed, const PrimitiveNOPtrVector& primMappings,
	                const PrimitivePartition::ClassifierValueType& clsVal);

	void addShape(const prt::InitialShape* is, AttributeMapBuilderUPtr&& amb, AttributeMapUPtr&& ruleAttr);

	InitialShapeBuilderVector& getInitialShapeBuilders() { return mInitialShapeBuilders; }
	int32_t getInitialShapeRandomSeed(size_t isIdx) const { return mRandomSeeds[isIdx]; }
	InitialShapeBuilderUPtr& getInitialShapeBuilder(size_t isIdx) { return mInitialShapeBuilders[isIdx]; }
	const PrimitiveNOPtrVector& getPrimitiveMapping(size_t isIdx) const { return mPrimitiveMapping[isIdx]; }

	AttributeMapBuilderVector& getRuleAttributeMapBuilders() { return mRuleAttributeBuilders; }
	const AttributeMapBuilderVector& getRuleAttributeMapBuilders() const { return mRuleAttributeBuilders; }

	const std::wstring& getInitialShapeName(size_t isIdx) const;
	const InitialShapeNOPtrVector& getInitialShapes() const { return mInitialShapes; }

	bool isValid() const;

private:
	std::vector<PrimitiveNOPtrVector> mPrimitiveMapping;

	InitialShapeBuilderVector         mInitialShapeBuilders;
	InitialShapeNOPtrVector           mInitialShapes;

	AttributeMapBuilderVector         mRuleAttributeBuilders;
	AttributeMapVector                mRuleAttributes;

	GenerateNodeParams::GroupCreation mGroupCreation = GenerateNodeParams::GroupCreation::NONE;
	std::vector<std::wstring>         mInitialShapeNames;
	std::wstring                      mNamePrefix;

	std::vector<int32_t>              mRandomSeeds;
};
