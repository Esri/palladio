#include "PrimitiveClassifier.h"


PrimitiveClassifier::PrimitiveClassifier(SOP_Node* node, const OP_Context& context) {
	get(node, context);
}

void PrimitiveClassifier::updateFromPrimitive(PrimitiveClassifier& derived, const GA_Detail* detail, const GA_Primitive* p) const {
	const GA_ROAttributeRef primClsAttrNameRef = detail->findPrimitiveAttribute(CE_PRIM_CLS_NAME);
	const GA_ROAttributeRef primClsAttrTypeRef = detail->findPrimitiveAttribute(CE_PRIM_CLS_TYPE);

	if (primClsAttrNameRef.isValid() && primClsAttrTypeRef.isValid()) {
		const GA_ROHandleS nameH(primClsAttrNameRef);
		derived.name = nameH.get(p->getMapOffset());

		const GA_ROHandleI typeH(primClsAttrTypeRef);
		derived.type = static_cast<GA_StorageClass>(typeH.get(p->getMapOffset()));
	}
	else {
		derived.name = this->name;
		derived.type = this->type;
	}
}

void PrimitiveClassifier::get(SOP_Node* node, const OP_Context& context) {
	const fpreal now = context.getTime();

	// -- primitive classifier attr name
	UT_String utNextClsAttrName;
	node->evalString(utNextClsAttrName, AssignNodeParams::PARAM_NAME_PRIM_CLS_ATTR.getToken(), 0, now);
	if (utNextClsAttrName.length() > 0)
		name.adopt(utNextClsAttrName);
	else
		node->setString(name, CH_STRING_LITERAL, AssignNodeParams::PARAM_NAME_PRIM_CLS_ATTR.getToken(), 0, now);

	// -- primitive classifier attr type
	const auto typeChoice = node->evalInt(AssignNodeParams::PARAM_NAME_PRIM_CLS_TYPE.getToken(), 0, now);
	switch (typeChoice) {
		case 0: type = GA_STORECLASS_STRING; break;
		case 1: type = GA_STORECLASS_INT;    break;
		case 2: type = GA_STORECLASS_FLOAT;  break;
		default: break;
	}
}

void PrimitiveClassifier::setupAttributeHandles(GU_Detail* detail) {
	GA_RWAttributeRef clsAttrNameRef(detail->addStringTuple(GA_ATTRIB_PRIMITIVE, CE_PRIM_CLS_NAME, 1));
	clsAttrNameH.bind(clsAttrNameRef);

	GA_RWAttributeRef clsTypeRef(detail->addIntTuple(GA_ATTRIB_PRIMITIVE, CE_PRIM_CLS_TYPE, 1));
	clsTypeH.bind(clsTypeRef);
}

void PrimitiveClassifier::put(const GA_Primitive* primitive) const {
	const GA_Offset& off = primitive->getMapOffset();
	clsAttrNameH.set(off, name);
	clsTypeH.set(off, type);
}
