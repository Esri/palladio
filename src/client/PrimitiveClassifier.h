#pragma once

#include "NodeParameter.h"

#include "SOP/SOP_Node.h"
#include "OP/OP_Context.h"
#include "GA/GA_Handle.h"
#include "GA/GA_Types.h"
#include "UT/UT_String.h"


const UT_String CE_PRIM_CLS_NAME = "cePrimClsName";
const UT_String CE_PRIM_CLS_TYPE = "cePrimClsType";

class PrimitiveClassifier {
public:
	UT_String       name;
	GA_StorageClass type = AssignNodeParams::DEFAULT_PRIM_CLS_TYPE_VALUE;

	GA_RWHandleS    clsAttrNameH;
	GA_RWHandleI    clsTypeH;

	PrimitiveClassifier() = default;
	PrimitiveClassifier(SOP_Node* node, const OP_Context& context);
	PrimitiveClassifier(const PrimitiveClassifier&) = delete;
	~PrimitiveClassifier() = default;

	void updateFromPrimitive(PrimitiveClassifier& derived, const GA_Detail* detail, const GA_Primitive* p) const;

	void get(SOP_Node* node, const OP_Context& context);
	void setupAttributeHandles(GU_Detail* detail);
	void put(const GA_Primitive* primitive) const;
};
