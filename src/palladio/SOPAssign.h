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

#include "PRTContext.h"
#include "ShapeConverter.h"

#include "SOP/SOP_Node.h"

#include <variant>

class SOPAssign : public SOP_Node {
public:
	SOPAssign(const PRTContextUPtr& pCtx, OP_Network* net, const char* name, OP_Operator* op);
	~SOPAssign() override = default;

	const PRTContextUPtr& getPRTCtx() const {
		return mPRTCtx;
	}
	const std::filesystem::path& getRPK() const {
		return mShapeConverter->mDefaultMainAttributes.mRPK;
	}
	const std::wstring& getStyle() const {
		return mShapeConverter->mDefaultMainAttributes.mStyle;
	}

	void updateDefaultCGAAttributes(const ShapeData& shapeData);
	void updatePrimitiveAttributes(GU_Detail* detail);
	void buildUI(GU_Detail* detail, ShapeData& shapeData, const ShapeConverterUPtr& shapeConverter,
	                        const PRTContextUPtr& prtCtx, std::string& errors);
	void opChanged(OP_EventType reason, void* data = nullptr) override;
	bool load(UT_IStream& is, const char* extension, const char* path) override;

protected:
	OP_ERROR cookMySop(OP_Context& context) override;

private:
	const PRTContextUPtr& mPRTCtx;
	ShapeConverterUPtr mShapeConverter;
	bool mWasJustLoaded = false;

public:
	using CGAAttributeValueType = std::variant<std::wstring, double, bool, std::vector<std::wstring>, std::vector<double>, std::vector<bool>>;
	using CGAAttributeValueMap = std::map<std::wstring, CGAAttributeValueType>;
	CGAAttributeValueMap mDefaultCGAAttributes;
};
