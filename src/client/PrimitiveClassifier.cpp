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
	UT_String pcn = AssignNodeParams::getPrimClsName(node, now);
	pcn.trimBoundingSpace();
	if (pcn.length() > 0)
		name.adopt(pcn);
	else
		AssignNodeParams::setPrimClsName(node, name, now);

	// -- primitive classifier attr type
	type = AssignNodeParams::getPrimClsType(node, now);
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
