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
#include "LogHandler.h"

#include "SOP/SOP_Node.h"

#include "BoostRedirect.h"
#include PLD_BOOST_INCLUDE(/variant.hpp)


class SOPAssign : public SOP_Node {
public:
	SOPAssign(const PRTContextUPtr& pCtx, OP_Network* net, const char* name, OP_Operator* op);
	~SOPAssign() override = default;

	const PRTContextUPtr& getPRTCtx() const {
		return mPRTCtx;
	}
	const PLD_BOOST_NS::filesystem::path& getRPK() const {
		return mShapeConverter->mDefaultMainAttributes.mRPK;
	}

	void opChanged(OP_EventType reason, void* data = nullptr) override;

protected:
	OP_ERROR cookMySop(OP_Context& context) override;
	void captureOverridableAttributes(const ShapeData& shapeData);

	bool updateParmsFlags() override {
		bool changed = SOP_Node::updateParmsFlags();

		// TODO: hide non-used value type child parms of ATTRIBUTES_OVERRIDE

		return changed;
	}

private:
	const PRTContextUPtr& mPRTCtx;
	ShapeConverterUPtr mShapeConverter;

public:
	using AttributeValueType = PLD_BOOST_NS::variant<std::wstring, double, bool>;
	using AttributeValueMap = std::map<std::wstring, AttributeValueType>;
	AttributeValueMap mOverridableAttributes;
};
