#include "client/InitialShapeContext.h"

#include "GU/GU_Detail.h"

namespace {

const UT_String CE_SHAPE_CLS_NAME = "ceShapeClsName";

} // namespace


namespace p4h {

InitialShapeContext::InitialShapeContext(GU_Detail* detail) {
	get(detail);
}

void InitialShapeContext::get(GU_Detail* detail) {
//	GA_ROAttributeRef ar(detail->findPrimitiveAttribute(CE_SHAPE_CLS_NAME));
//	GA_ROHandleS ah(detail, GA_ATTRIB_PRIMITIVE, ar->getName());
//	mShapeClsAttrName = ah.get

}

void InitialShapeContext::put(GU_Detail* detail) {
	GA_RWAttributeRef ceShapeClsName(detail->findPrimitiveAttribute(CE_SHAPE_CLS_NAME));
}

} // namespace p4h

