/*
 * Copyright 2014-2019 Esri R&D Zurich and VRBN
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

#include "GA/GA_Handle.h"
#include "GA/GA_Types.h"
#include "OP/OP_Context.h"
#include "SOP/SOP_Node.h"
#include "UT/UT_String.h"

const UT_String PLD_PRIM_CLS_NAME = "pldPrimClsName";

class PrimitiveClassifier {
public:
	UT_String name;
	GA_RWHandleS clsAttrNameH;

	PrimitiveClassifier() = default;
	PrimitiveClassifier(SOP_Node* node, const GA_Detail* detail, const OP_Context& context);
	PrimitiveClassifier(const PrimitiveClassifier&) = delete;
	~PrimitiveClassifier() = default;

	void updateFromPrimitive(PrimitiveClassifier& derived, const GA_Detail* detail, const GA_Primitive* p) const;

	void get(SOP_Node* node, const GA_Detail* detail, const OP_Context& context);
	void setupAttributeHandles(GU_Detail* detail);
	void put(const GA_Primitive* primitive) const;
};
