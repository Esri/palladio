/*
 * Copyright 2014-2020 Esri R&D Zurich and VRBN
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

#include "PrimitivePartition.h"
#include "LogHandler.h"
#include "PrimitiveClassifier.h"

namespace {

constexpr bool DBG = false;
constexpr int32 INVALID_CLS_VALUE = -1;

} // namespace

PrimitivePartition::PrimitivePartition(const GA_Detail* detail, const PrimitiveClassifier& primCls) {
	const GA_Primitive* prim = nullptr;
	GA_FOR_ALL_PRIMITIVES(detail, prim) {
		add(detail, primCls, prim);
	}
}

void PrimitivePartition::add(const GA_Detail* detail, const PrimitiveClassifier& primCls, const GA_Primitive* p) {
	if (DBG)
		LOG_DBG << "      adding prim: " << detail->primitiveIndex(p->getMapOffset());

	PrimitiveClassifier updatedPrimCls;
	primCls.updateFromPrimitive(updatedPrimCls, detail, p);

	// try to read primitive classifier attr handle
	GA_ROAttributeRef primClsAttrRef;
	if (updatedPrimCls.name.length() > 0) { // we have a valid primitive classifier attribute name
		const GA_ROAttributeRef r(detail->findPrimitiveAttribute(updatedPrimCls.name));
		if (r.isValid()) {
			const auto& sc = r->getStorageClass();
			if (sc == GA_STORECLASS_STRING || sc == GA_STORECLASS_INT)
				primClsAttrRef = r; // we found the primitive classifier attribute itself
			else
				LOG_WRN << "Ignoring primitive classifier '" << r->getName()
				        << "', it is neither of type string or int";
		}
	}

	// try to read actual attr value and classify primitive
	if (primClsAttrRef.isInvalid()) {
		mPrimitives[INVALID_CLS_VALUE].push_back(p);
		if (DBG)
			LOG_DBG << "       missing cls name: adding prim to fallback shape!";
	}
	else if (primClsAttrRef.isInt()) {
		const GA_ROHandleI av(primClsAttrRef);
		if (av.isValid()) {
			const int32 v = av.get(p->getMapOffset());
			if (DBG)
				LOG_DBG << "        got int classifier value: " << v;
			mPrimitives[v].push_back(p);
		}
		else {
			if (DBG)
				LOG_DBG << "        int: invalid handle!";
		}
	}
	else if (primClsAttrRef.isString()) {
		const GA_ROHandleS av(primClsAttrRef);
		if (av.isValid()) {
			const char* v = av.get(p->getMapOffset());
			if (v) {
				if (DBG)
					LOG_DBG << "        got string classifier value: " << v;
				mPrimitives[UT_String(v)].push_back(p);
			}
			else {
				mPrimitives[INVALID_CLS_VALUE].push_back(p);
				LOG_WRN << "primitive classifier attribute has empty string value -> fallback shape";
			}
		}
		else {
			if (DBG)
				LOG_DBG << "        string: invalid handle!";
		}
	}
	else {
		if (DBG)
			LOG_DBG << "        wrong type!";
	}
}
