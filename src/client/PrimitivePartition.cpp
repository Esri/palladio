#include "PrimitivePartition.h"
#include "PrimitiveClassifier.h"
#include "LogHandler.h"


namespace {

constexpr bool  DBG               = false;
constexpr int32 INVALID_CLS_VALUE = -1;

} // namespace


PrimitivePartition::PrimitivePartition(const GA_Detail* detail, const PrimitiveClassifier& primCls) {
	const GA_Primitive* prim = nullptr;
	GA_FOR_ALL_PRIMITIVES(detail, prim) {
		add(detail, primCls, prim);
	}
}

void PrimitivePartition::add(const GA_Detail* detail, const PrimitiveClassifier& primCls, const GA_Primitive* p) {
	if (DBG) LOG_DBG << "      adding prim: " << detail->primitiveIndex(p->getMapOffset());

	PrimitiveClassifier updatedPrimCls;
	primCls.updateFromPrimitive(updatedPrimCls, detail, p);

	// try to read primitive classifier attr name and type
	GA_ROAttributeRef primClsAttrRef;
	if (updatedPrimCls.name.length() > 0) { // we have a valid primitive classifier attribute name
		const GA_ROAttributeRef r(detail->findPrimitiveAttribute(updatedPrimCls.name));
		if (r.isValid() && r->getStorageClass() == updatedPrimCls.type)
			primClsAttrRef = r; // we found the primitive classifier attribute itself
	}

	if (primClsAttrRef.isInvalid()) {
		mPrimitives[INVALID_CLS_VALUE].push_back(p);
		if (DBG) LOG_DBG << "       missing cls name: adding prim to fallback shape!";
	}
	else if ((updatedPrimCls.type == GA_STORECLASS_FLOAT) && primClsAttrRef.isFloat()) {
		const GA_ROHandleF av(primClsAttrRef);
		if (av.isValid()) {
			const fpreal32 v = av.get(p->getMapOffset());
			if (DBG) LOG_DBG << "        got float classifier value: " << v;
			mPrimitives[v].push_back(p);
		}
		else {
			if (DBG) LOG_DBG << "        float: invalid handle!";
		}
	}
	else if ((updatedPrimCls.type == GA_STORECLASS_INT) && primClsAttrRef.isInt()) {
		const GA_ROHandleI av(primClsAttrRef);
		if (av.isValid()) {
			const int32 v = av.get(p->getMapOffset());
			if (DBG) LOG_DBG << "        got int classifier value: " << v;
			mPrimitives[v].push_back(p);
		}
		else {
			if (DBG) LOG_DBG << "        int: invalid handle!";
		}
	}
	else if ((updatedPrimCls.type == GA_STORECLASS_STRING) && primClsAttrRef.isString()) {
		const GA_ROHandleS av(primClsAttrRef);
		if (av.isValid()) {
			const char* v = av.get(p->getMapOffset());
			if (v) {
				if (DBG) LOG_DBG << "        got string classifier value: " << v;
				mPrimitives[UT_String(v)].push_back(p);
			}
			else {
				mPrimitives[INVALID_CLS_VALUE].push_back(p);
				LOG_WRN << "primitive classifier attribute has empty string value -> fallback shape";
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
