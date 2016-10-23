#include "client/InitialShapeContext.h"

#include "GU/GU_Detail.h"

namespace {

const UT_String CE_SHAPE_CLS_NAME = "ceShapeClsName";
const UT_String CE_SHAPE_CLS_TYPE = "ceShapeClsType"; // ordinal of GA_StorageClass
const UT_String CE_SHAPE_RPK      = "ceShapeRPK";

} // namespace


namespace p4h {

InitialShapeContext::InitialShapeContext(GU_Detail* detail) {
//	get(detail);
}

void InitialShapeContext::put(GU_Detail* detail) {
	GA_RWAttributeRef clsName(detail->addStringTuple(GA_ATTRIB_PRIMITIVE, CE_SHAPE_CLS_NAME, 1));
	GA_RWHandleS clsNameH(detail, GA_ATTRIB_PRIMITIVE, clsName->getName());

	GA_RWAttributeRef clsType(detail->addIntTuple(GA_ATTRIB_PRIMITIVE, CE_SHAPE_CLS_TYPE, 1));
	GA_RWHandleI clsTypeH(detail, GA_ATTRIB_PRIMITIVE, clsType->getName());

	GA_RWAttributeRef rpk(detail->addStringTuple(GA_ATTRIB_PRIMITIVE, CE_SHAPE_RPK, 1));
	GA_RWHandleS rpkH(detail, GA_ATTRIB_PRIMITIVE, rpk->getName());

	GA_Primitive* prim = nullptr;
	GA_FOR_ALL_PRIMITIVES(detail, prim) {
		const GA_Offset& off = prim->getMapOffset();
		clsNameH.set(off, mShapeClsAttrName.c_str());
		clsTypeH.set(off, mShapeClsType);
		// TODO: set shapeClsName

		rpkH.set(off, mRPK.string().c_str());
		// TODO: primary attrs (startRule etc)

		size_t keyCount = 0;
		const wchar_t* const* cKeys = mRuleAttributeValues->getKeys(&keyCount);
		for (size_t k = 0; k < keyCount; k++) {
			const wchar_t* key = cKeys[k];
			std::string nKey = utils::toOSNarrowFromUTF16(key);
			switch (mRuleAttributeValues->getType(key)) {
				case prt::AttributeMap::PT_FLOAT: {
					GA_WOAttributeRef ar(detail->addFloatTuple(GA_ATTRIB_PRIMITIVE, nKey.c_str(), 1));
					GA_RWHandleD av(ar);
					double userAttrValue = mUserAttributeValues->getFloat(key);
					av.set(prim->getMapOffset(), userAttrValue);
					break;
				}
				case prt::AttributeMap::PT_BOOL: {
					GA_WOAttributeRef ar(detail->addIntTuple(GA_ATTRIB_PRIMITIVE, nKey.c_str(), 1));
					GA_RWHandleI av(ar);
					bool userAttrValue = mUserAttributeValues->getBool(key);
					av.set(prim->getMapOffset(), userAttrValue ? 1 : 0);
					break;
				}
				case prt::AttributeMap::PT_STRING: {
					GA_WOAttributeRef ar(detail->addStringTuple(GA_ATTRIB_PRIMITIVE, nKey.c_str(), 1));
					GA_RWHandleS av(ar);
					const wchar_t* userAttrValue = mUserAttributeValues->getString(key);
					std::string nVal = utils::toOSNarrowFromUTF16(userAttrValue);
					av.set(prim->getMapOffset(), nVal.c_str());
					break;
				}
				default: {
					LOG_ERR << "Array attribute support not implemented yet";
				}
			} // switch type
		} // for user keys
	} // for all prims
}

GA_ROAttributeRef InitialShapeContext::getClsName(const GU_Detail* detail) {
	return GA_ROAttributeRef(detail->findPrimitiveAttribute(CE_SHAPE_CLS_NAME));
}

GA_ROAttributeRef InitialShapeContext::getClsType(const GU_Detail* detail) {
	return GA_ROAttributeRef(detail->findPrimitiveAttribute(CE_SHAPE_CLS_TYPE));
}

//std::vector<InitialShapeContextUPtr> InitialShapeContext::get(GU_Detail* detail) {
//	GA_ROAttributeRef ar(detail->findPrimitiveAttribute(CE_SHAPE_CLS_NAME));
//	GA_ROHandleS ah(detail, GA_ATTRIB_PRIMITIVE, ar->getName());
//
//	std::map<boost::filesystem::path, InitialShapeContextUPtr> ctx;
//
//	// read the value from the first primitive
//	// loop over prims, put into multiple ISCs, one per RPK
//	//mShapeClsAttrName = ah.get()
//}

} // namespace p4h

