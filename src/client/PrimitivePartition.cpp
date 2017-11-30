#include "PrimitivePartition.h"
#include "logging.h"


namespace {

constexpr bool  DBG = false;
constexpr int32 INVALID_CLS_VALUE{-1};

} // namespace


PrimitivePartition::PrimitivePartition(const GU_Detail* detail, const UT_String& assignShpClsAttrName, const GA_StorageClass& assignShpClsAttrType) {
	const GA_Primitive* prim = nullptr;
	GA_FOR_ALL_PRIMITIVES(detail, prim) {
		add(detail, prim, assignShpClsAttrName, assignShpClsAttrType);
	}
}

/**
 * @param assignShpClsAttrName if empty, we assume generate mode and get the shape classifier attr name/type from the detail itself
 */
void PrimitivePartition::add(const GA_Detail* detail, const GA_Primitive* p, const UT_String& assignShpClsAttrName, const GA_StorageClass& assignShpClsAttrType) {
	if (DBG) LOG_DBG << "      adding prim: " << detail->primitiveIndex(p->getMapOffset());

	// get shape classifier attr name and type
	UT_String shpClsAttrName;
	GA_StorageClass shpClsAttrType;
	if (assignShpClsAttrName.length() == 0) { // generate mode
		GA_ROAttributeRef shpClsAttrNameRef = detail->findPrimitiveAttribute(CE_SHAPE_CLS_NAME);
		GA_ROAttributeRef shpClsAttrTypeRef = detail->findPrimitiveAttribute(CE_SHAPE_CLS_TYPE);

		if (shpClsAttrNameRef.isValid() && shpClsAttrTypeRef.isValid()) {
			GA_ROHandleS shpClsAttrNameH(shpClsAttrNameRef);
			shpClsAttrName = shpClsAttrNameH.get(p->getMapOffset());

			GA_ROHandleI shpClsAttrTypeH(shpClsAttrTypeRef);
			shpClsAttrType = static_cast<GA_StorageClass>(shpClsAttrTypeH.get(p->getMapOffset()));
		}
	}
	else { // assign mode
		shpClsAttrName = assignShpClsAttrName;
		shpClsAttrType = assignShpClsAttrType;
	}

	// try to read shape classifier attr name and type
	GA_ROAttributeRef shpClsAttrRef;
	if (shpClsAttrName.length() > 0) { // we have a valid shape classifier attribute name
		GA_ROAttributeRef r(detail->findPrimitiveAttribute(shpClsAttrName.buffer()));
		if (r.isValid() && r->getStorageClass() == shpClsAttrType)
			shpClsAttrRef = r; // we found the shape classifier attribute itself
	}

	if (shpClsAttrRef.isInvalid()) {
		mPrimitives[INVALID_CLS_VALUE].push_back(p);
		if (DBG) LOG_DBG << "       missing cls name: adding prim to fallback shape!";
	}
	else if ((shpClsAttrType == GA_STORECLASS_FLOAT) && shpClsAttrRef.isFloat()) {
		GA_ROHandleF av(shpClsAttrRef);
		if (av.isValid()) {
			fpreal32 v = av.get(p->getMapOffset());
			if (DBG) LOG_DBG << "        got float classifier value: " << v;
			mPrimitives[v].push_back(p);
		}
		else {
			if (DBG) LOG_DBG << "        float: invalid handle!";
		}
	}
	else if ((shpClsAttrType == GA_STORECLASS_INT) && shpClsAttrRef.isInt()) {
		GA_ROHandleI av(shpClsAttrRef);
		if (av.isValid()) {
			int32 v = av.get(p->getMapOffset());
			if (DBG) LOG_DBG << "        got int classifier value: " << v;
			mPrimitives[v].push_back(p);
		}
		else {
			if (DBG) LOG_DBG << "        int: invalid handle!";
		}
	}
	else if ((shpClsAttrType == GA_STORECLASS_STRING) && shpClsAttrRef.isString()) {
		GA_ROHandleS av(shpClsAttrRef);
		if (av.isValid()) {
			const char* v = av.get(p->getMapOffset());
			if (v) {
				if (DBG) LOG_DBG << "        got string classifier value: " << v;
				mPrimitives[UT_String(v)].push_back(p);
			}
			else {
				mPrimitives[INVALID_CLS_VALUE].push_back(p);
				LOG_WRN << "shape classifier attribute has empty string value -> fallback shape";
			}
		}
		else {
			if (DBG) LOG_DBG << "        string: invalid handle!";
		}
	}
	else {
		if (DBG) LOG_DBG << "        wrong type!";
	}
}
