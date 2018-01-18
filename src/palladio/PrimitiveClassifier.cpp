#include "PrimitiveClassifier.h"


PrimitiveClassifier::PrimitiveClassifier(SOP_Node* node, const GA_Detail* detail, const OP_Context& context) {
	get(node, detail, context);
}

void PrimitiveClassifier::updateFromPrimitive(PrimitiveClassifier& derived, const GA_Detail* detail, const GA_Primitive* p) const {
	const GA_ROAttributeRef primClsAttrNameRef = detail->findPrimitiveAttribute(PLD_PRIM_CLS_NAME);
	if (primClsAttrNameRef.isValid()) {
		const GA_ROHandleS nameH(primClsAttrNameRef);
		derived.name = nameH.get(p->getMapOffset());
	}
	else {
		derived.name = this->name;
	}
}

void PrimitiveClassifier::get(SOP_Node* node, const GA_Detail* detail, const OP_Context& context) {
	const fpreal now = context.getTime();

	UT_String pcn = AssignNodeParams::getPrimClsName(node, now);
	pcn.trimBoundingSpace();
	if (pcn.length() > 0)
		name.adopt(pcn);
	else
		AssignNodeParams::setPrimClsName(node, name, now);
}

void PrimitiveClassifier::setupAttributeHandles(GU_Detail* detail) {
	GA_RWAttributeRef clsAttrNameRef(detail->addStringTuple(GA_ATTRIB_PRIMITIVE, PLD_PRIM_CLS_NAME, 1));
	clsAttrNameH.bind(clsAttrNameRef);
}

void PrimitiveClassifier::put(const GA_Primitive* primitive) const {
	const GA_Offset& off = primitive->getMapOffset();
	clsAttrNameH.set(off, name);
}
