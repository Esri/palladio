#pragma once

#include "SOP/SOP_Node.h"
#include "OP/OP_Context.h"
#include "GA/GA_Handle.h"
#include "GA/GA_Types.h"
#include "UT/UT_String.h"


const UT_String PLD_PRIM_CLS_NAME = "pldPrimClsName";

class PrimitiveClassifier {
public:
	UT_String    name;
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
