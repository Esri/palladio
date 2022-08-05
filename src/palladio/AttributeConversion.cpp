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

#include "AttributeConversion.h"
#include "LRUCache.h"
#include "LogHandler.h"
#include "MultiWatch.h"

#include <mutex>
#include <bitset>

#include "UT/UT_VarEncode.h"

namespace {

constexpr bool DBG = false;

namespace StringConversionCaches {
LockedLRUCache<std::wstring, UT_String> toPrimAttr(1 << 12);
}

void setHandleRange(const GA_IndexMap& indexMap, GA_RWHandleC& handle, GA_Offset start, GA_Size size, int component,
                    const bool& value) {
	constexpr int8_t valFalse = 0;
	constexpr int8_t valTrue = 1;
	const int8_t hv = value ? valTrue : valFalse;
	handle.setBlock(start, size, &hv, 0, component);
	if (DBG)
		LOG_DBG << "bool attr: range = [" << start << ", " << start + size << "): " << handle.getAttribute()->getName()
		        << " = " << value;
}

void setHandleRange(const GA_IndexMap& indexMap, GA_RWHandleI& handle, GA_Offset start, GA_Size size, int component,
                    const int32_t& value) {
	handle.setBlock(start, size, &value, 0, component);
	if (DBG)
		LOG_DBG << "int attr: range = [" << start << ", " << start + size << "): " << handle.getAttribute()->getName()
		        << " = " << value;
}

void setHandleRange(const GA_IndexMap& indexMap, GA_RWHandleF& handle, GA_Offset start, GA_Size size, int component,
                    const double& value) {
	const auto hv = static_cast<fpreal32>(value);
	handle.setBlock(start, size, &hv, 0, component); // using stride = 0 to always set the same value
	if (DBG)
		LOG_DBG << "float attr: component = " << component << ", range = [" << start << ", " << start + size
		        << "): " << handle.getAttribute()->getName() << " = " << value;
}

void setHandleRange(const GA_IndexMap& indexMap, GA_RWBatchHandleS& handle, GA_Offset start, GA_Size size,
                    int component, const std::wstring& value) {
	const UT_String attrValue = [&value]() {
		const auto sh = StringConversionCaches::toPrimAttr.get(value);
		if (sh)
			return sh.value();
		const std::string nv = toOSNarrowFromUTF16(value);
		UT_String hv(UT_String::ALWAYS_DEEP, nv); // ensure owning UT_String inside cache
		StringConversionCaches::toPrimAttr.insert(value, hv);
		return hv;
	}();

	const GA_Range range(indexMap, start, start + size);
	handle.set(range, component, attrValue);

	if (DBG)
		LOG_DBG << "string attr: range = [" << start << ", " << start + size
		        << "): " << handle.getAttribute()->getName() << " = " << attrValue;
}

void setHandleRange(const GA_IndexMap& indexMap, GA_RWHandleDA& handle, GA_Offset start, GA_Size size,
                    const double* ptr, size_t ptrSize) {
	UT_Fpreal64Array hv;
	hv.append(ptr, ptrSize);
	for (GA_Offset off = start; off < start + size; off++)
		handle.set(off, hv);
	if (DBG)
		LOG_DBG << "float array attr: range = [" << start << ", " << start + size
		        << "): " << handle.getAttribute()->getName() << " = " << hv;
}

void setHandleRange(const GA_IndexMap& indexMap, GA_RWHandleIA& handle, GA_Offset start, GA_Size size,
                    const int32_t* ptr, size_t ptrSize) {
	UT_Int32Array hv;
	hv.append(ptr, ptrSize);
	for (GA_Offset off = start; off < start + size; off++)
		handle.set(off, hv);
	if (DBG)
		LOG_DBG << "int array attr: range = [" << start << ", " << start + size
		        << "): " << handle.getAttribute()->getName() << " = " << hv;
}

void setHandleRange(const GA_IndexMap& indexMap, GA_RWHandleIA& handle, GA_Offset start, GA_Size size, const bool* ptr,
                    size_t ptrSize) {
	UT_IntArray hv(ptrSize, ptrSize); // there is no UT_Int8Array
	for (size_t i = 0; i < ptrSize; i++)
		hv[i] = ptr[i] ? 1u : 0u;
	for (GA_Offset off = start; off < start + size; off++)
		handle.set(off, hv);
	if (DBG)
		LOG_DBG << "bool array attr: range = [" << start << ", " << start + size
		        << "): " << handle.getAttribute()->getName() << " = " << hv;
}

void setHandleRange(const GA_IndexMap& indexMap, GA_RWHandleSA& handle, GA_Offset start, GA_Size size,
                    wchar_t const* const* ptr, size_t ptrSize) {
	UT_StringArray hv(ptrSize, ptrSize);
	for (size_t i = 0; i < ptrSize; i++)
		hv[i] = UT_StringHolder(toOSNarrowFromUTF16(ptr[i]));
	for (GA_Offset off = start; off < start + size; off++)
		handle.set(off, hv);
	if (DBG)
		LOG_DBG << "string array attr: range = [" << start << ", " << start + size
		        << "): " << handle.getAttribute()->getName() << " = " << hv;
}

size_t getAttributeCardinality(const prt::AttributeMap* attrMap, const std::wstring& key,
                               const prt::Attributable::PrimitiveType& type) {
	size_t cardinality = -1;
	switch (type) {
		case prt::Attributable::PT_STRING_ARRAY: {
			attrMap->getStringArray(key.c_str(), &cardinality);
			break;
		}
		case prt::Attributable::PT_FLOAT_ARRAY: {
			attrMap->getFloatArray(key.c_str(), &cardinality);
			break;
		}
		case prt::Attributable::PT_INT_ARRAY: {
			attrMap->getIntArray(key.c_str(), &cardinality);
			break;
		}
		case prt::Attributable::PT_BOOL_ARRAY: {
			attrMap->getBoolArray(key.c_str(), &cardinality);
			break;
		}
		default: {
			cardinality = 1;
			break;
		}
	}
	return cardinality;
}

bool isArrayAttribute(const GA_ROAttributeRef& ar) {
	return (ar.getAIFNumericArray() != nullptr) || (ar.getAIFSharedStringArray() != nullptr);
}

} // namespace

namespace AttributeConversion {

bool FromHoudini::convert(const GA_ROAttributeRef& ar, const GA_Offset& offset, const std::wstring& name) {
	if (isArrayAttribute(ar))
		return handleArray(ar, offset, name);
	else
		return handleScalar(ar, offset, name);
}

bool FromHoudini::handleScalar(const GA_ROAttributeRef& ar, const GA_Offset& offset, const std::wstring& name) {
	bool conversionResult = true;
	switch (ar.getStorageClass()) {
		case GA_STORECLASS_FLOAT: {
			GA_ROHandleD av(ar);
			if (av.isValid()) {
				double v = av.get(offset);
				if (DBG)
					LOG_DBG << "   prim float attr: " << ar->getName() << " = " << v;
				mBuilder.setFloat(name.c_str(), v);
			}
			break;
		}
		case GA_STORECLASS_STRING: {
			GA_ROHandleS av(ar);
			if (av.isValid()) {
				const char* v = av.get(offset);
				const std::wstring wv = toUTF16FromOSNarrow(v);
				if (DBG)
					LOG_DBG << "   prim string attr: " << ar->getName() << " = " << v;
				mBuilder.setString(name.c_str(), wv.c_str());
			}
			break;
		}
		case GA_STORECLASS_INT: {
			if (ar.getAIFTuple()->getStorage(ar.get()) == GA_STORE_INT8) {
				GA_ROHandleI av(ar);
				if (av.isValid()) {
					const int v = av.get(offset);
					const bool bv = (v > 0);
					if (DBG)
						LOG_DBG << "   prim bool attr: " << ar->getName() << " = " << bv;
					mBuilder.setBool(name.c_str(), bv);
				}
			}
			else {
				GA_ROHandleI av(ar);
				if (av.isValid()) {
					const int v = av.get(offset);
					if (DBG)
						LOG_DBG << "   prim int attr: " << ar->getName() << " = " << v;
					mBuilder.setInt(name.c_str(), v);
				}
			}
			break;
		}
		default: {
			LOG_WRN << "prim attr " << ar->getName() << ": unsupported storage class";
			conversionResult = false;
			break;
		}
	}
	return conversionResult;
}

bool FromHoudini::handleArray(const GA_ROAttributeRef& ar, const GA_Offset& offset, const std::wstring& name) {
	bool conversionResult = true;
	switch (ar.getStorageClass()) {
		case GA_STORECLASS_FLOAT: {
			GA_ROHandleDA av(ar);
			if (av.isValid()) {
				UT_Fpreal64Array v;
				av.get(offset, v);
				if (DBG)
					LOG_DBG << "   prim float array attr: " << ar->getName() << " = " << v;
				mBuilder.setFloatArray(name.c_str(), v.data(), v.size());
			}
			break;
		}
		case GA_STORECLASS_STRING: {
			GA_ROHandleSA av(ar);
			if (av.isValid()) {
				UT_StringArray v;
				av.get(offset, v);
				if (DBG)
					LOG_DBG << "   prim string array attr: " << ar->getName() << " = " << v;

				std::vector<std::wstring> wstrings(v.size());
				std::vector<const wchar_t*> wstringPtrs(v.size());
				for (size_t i = 0; i < v.size(); i++) {
					wstrings[i] = toUTF16FromOSNarrow(v[i].toStdString());
					wstringPtrs[i] = wstrings[i].c_str();
				}
				mBuilder.setStringArray(name.c_str(), wstringPtrs.data(), wstringPtrs.size());
			}
			break;
		}
		case GA_STORECLASS_INT: {
			if (ar.getAIFNumericArray()->getStorage(ar.get()) == GA_STORE_INT8) {
				GA_ROHandleIA av(ar);
				if (av.isValid()) {
					UT_Int32Array v; // there is no U_Int8Array
					av.get(offset, v);
					if (DBG)
						LOG_DBG << "   prim bool array attr: " << ar->getName() << " = " << v;
					const std::unique_ptr<bool[]> vPtrs(new bool[v.size()]);
					for (size_t i = 0; i < v.size(); i++)
						vPtrs[i] = (v[i] > 0);
					mBuilder.setBoolArray(name.c_str(), vPtrs.get(), v.size());
				}
			}
			else {
				GA_ROHandleIA av(ar);
				if (av.isValid()) {
					UT_Int32Array v;
					av.get(offset, v);
					if (DBG)
						LOG_DBG << "   prim int array attr: " << ar->getName() << " = " << v;
					mBuilder.setIntArray(name.c_str(), v.data(), v.size());
				}
			}
			break;
		}
		default: {
			LOG_WRN << "prim attr " << ar->getName() << ": unsupported storage class";
			conversionResult = false;
			break;
		}
	}
	return conversionResult;
}

void ToHoudini::convert(const prt::AttributeMap* attrMap, const GA_Offset rangeStart, const GA_Size rangeSize,
                        ArrayHandling arrayHandling) {
	const GA_IndexMap& primIndexMap = mDetail->getIndexMap(GA_ATTRIB_PRIMITIVE);
	extractAttributeNames(attrMap);
	createAttributeHandles(arrayHandling == ArrayHandling::ARRAY);
	setAttributeValues(attrMap, primIndexMap, rangeStart, rangeSize);
}

void ToHoudini::extractAttributeNames(const prt::AttributeMap* attrMap) {
	size_t keyCount = 0;
	wchar_t const* const* keys = attrMap->getKeys(&keyCount);
	for (size_t k = 0; k < keyCount; k++) {
		wchar_t const* key = keys[k];

		ProtoHandle ph;
		ph.type = attrMap->getType(key);
		ph.key.assign(key);
		ph.cardinality = getAttributeCardinality(attrMap, ph.key, ph.type);
		addProtoHandle(mHandleMap, key, std::move(ph));
	}
}

void ToHoudini::createAttributeHandles(bool useArrayTypes) {
	WA("all");

	for (auto& hm : mHandleMap) {
		const auto& utKey = hm.first;
		const auto& type = hm.second.type;

		HandleType handle; // set to NoHandle by default
		assert(handle.index() == 0);
		switch (type) {
			case prt::Attributable::PT_BOOL:
			case prt::Attributable::PT_BOOL_ARRAY: {
				if (useArrayTypes && (type == prt::Attributable::PT_BOOL_ARRAY)) {
					GA_RWHandleIA h(
					        mDetail->addIntArray(GA_ATTRIB_PRIMITIVE, utKey, 1, nullptr, nullptr, GA_STORE_INT8));
					if (h.isValid())
						handle = h;
				}
				else {
					GA_RWHandleC h(mDetail->addIntTuple(GA_ATTRIB_PRIMITIVE, utKey, hm.second.cardinality,
					                                    GA_Defaults(0), nullptr, nullptr, GA_STORE_INT8));
					if (h.isValid())
						handle = h;
				}
				break;
			}
			case prt::Attributable::PT_FLOAT:
			case prt::Attributable::PT_FLOAT_ARRAY: {
				if (useArrayTypes && (type == prt::Attributable::PT_FLOAT_ARRAY)) {
					GA_RWHandleDA h(
					        mDetail->addFloatArray(GA_ATTRIB_PRIMITIVE, utKey, 1, nullptr, nullptr, GA_STORE_REAL64));
					if (h.isValid())
						handle = h;
				}
				else {
					GA_RWHandleF h(mDetail->addFloatTuple(GA_ATTRIB_PRIMITIVE, utKey, hm.second.cardinality));
					if (h.isValid())
						handle = h;
				}
				break;
			}
			case prt::Attributable::PT_INT:
			case prt::Attributable::PT_INT_ARRAY: {
				if (useArrayTypes && (type == prt::Attributable::PT_INT_ARRAY)) {
					GA_RWHandleIA h(
					        mDetail->addIntArray(GA_ATTRIB_PRIMITIVE, utKey, 1, nullptr, nullptr, GA_STORE_INT32));
					if (h.isValid())
						handle = h;
				}
				else {
					GA_RWHandleI h(mDetail->addIntTuple(GA_ATTRIB_PRIMITIVE, utKey, hm.second.cardinality));
					if (h.isValid())
						handle = h;
				}
				break;
			}
			case prt::Attributable::PT_STRING:
			case prt::Attributable::PT_STRING_ARRAY: {
				if (useArrayTypes && (type == prt::Attributable::PT_STRING_ARRAY)) {
					GA_RWHandleSA h(
					        mDetail->addStringArray(GA_ATTRIB_PRIMITIVE, utKey, 1, nullptr, nullptr, GA_STORE_STRING));
					if (h.isValid())
						handle = h;
				}
				else {
					GA_RWBatchHandleS h(mDetail->addStringTuple(GA_ATTRIB_PRIMITIVE, utKey, hm.second.cardinality));
					if (h.isValid())
						handle = h;
				}
				break;
			}
			default:
				if (DBG)
					LOG_DBG << "ignored: " << utKey;
				break;
		}

		if (handle.index() != 0) {
			hm.second.handleType = handle;
			if (DBG)
				LOG_DBG << "added attr handle " << utKey;
		}
		else if (DBG)
			LOG_DBG << "could not update handle for primitive attribute " << utKey;
	}
}

void ToHoudini::setAttributeValues(const prt::AttributeMap* attrMap, const GA_IndexMap& primIndexMap,
                                   const GA_Offset rangeStart, const GA_Size rangeSize) {
	for (auto& h : mHandleMap) {
		if (attrMap->hasKey(h.second.key.c_str())) {
			const HandleVisitor hv(h.second, attrMap, primIndexMap, rangeStart, rangeSize);
			std::visit(hv, h.second.handleType);
		}
	}
}

void ToHoudini::addProtoHandle(HandleMap& handleMap, const std::wstring& handleName, ProtoHandle&& ph) {
	WA("all");

	const UT_StringHolder& utName = NameConversion::toPrimAttr(handleName);
	if (DBG)
		LOG_DBG << "handle name conversion: handleName = " << handleName << ", utName = " << utName;
	handleMap.emplace(utName, std::move(ph));
}

void ToHoudini::HandleVisitor::operator()(GA_RWBatchHandleS& handle) const {
	if (protoHandle.type == prt::Attributable::PT_STRING) {
		wchar_t const* const v = attrMap->getString(protoHandle.key.c_str());
		if (v && std::wcslen(v) > 0) {
			setHandleRange(primIndexMap, handle, rangeStart, rangeSize, 0, std::wstring(v));
		}
	}
	else if (protoHandle.type == prt::Attributable::PT_STRING_ARRAY) {
		size_t arraySize = 0;
		wchar_t const* const* const v = attrMap->getStringArray(protoHandle.key.c_str(), &arraySize);
		for (size_t i = 0; i < arraySize; i++) {
			if (v && v[i] && std::wcslen(v[i]) > 0) {
				setHandleRange(primIndexMap, handle, rangeStart, rangeSize, i, std::wstring(v[i]));
			}
		}
	}
}

void ToHoudini::HandleVisitor::operator()(GA_RWHandleI& handle) const {
	if (protoHandle.type == prt::Attributable::PT_INT) {
		const int32_t v = attrMap->getInt(protoHandle.key.c_str());
		setHandleRange(primIndexMap, handle, rangeStart, rangeSize, 0, v);
	}
	else if (protoHandle.type == prt::Attributable::PT_INT_ARRAY) {
		LOG_ERR << "int arrays as tuples not yet implemented";
	}
}

void ToHoudini::HandleVisitor::operator()(GA_RWHandleC& handle) const {
	if (protoHandle.type == prt::Attributable::PT_BOOL) {
		const bool v = attrMap->getBool(protoHandle.key.c_str());
		setHandleRange(primIndexMap, handle, rangeStart, rangeSize, 0, v);
	}
	else if (protoHandle.type == prt::Attributable::PT_BOOL_ARRAY) {
		LOG_ERR << "bool arrays as tuples not yet implemented";
	}
}

void ToHoudini::HandleVisitor::operator()(GA_RWHandleF& handle) const {
	if (protoHandle.type == prt::Attributable::PT_FLOAT) {
		const auto v = attrMap->getFloat(protoHandle.key.c_str());
		setHandleRange(primIndexMap, handle, rangeStart, rangeSize, 0, v);
	}
	else if (protoHandle.type == prt::Attributable::PT_FLOAT_ARRAY) {
		size_t arraySize = 0;
		const double* const v = attrMap->getFloatArray(protoHandle.key.c_str(), &arraySize);
		for (size_t i = 0; i < arraySize; i++) {
			setHandleRange(primIndexMap, handle, rangeStart, rangeSize, i, v[i]);
		}
	}
}

void ToHoudini::HandleVisitor::operator()(GA_RWHandleDA& handle) const {
	size_t arraySize = 0;
	const double* const array = attrMap->getFloatArray(protoHandle.key.c_str(), &arraySize);
	setHandleRange(primIndexMap, handle, rangeStart, rangeSize, array, arraySize);
}

void ToHoudini::HandleVisitor::operator()(GA_RWHandleIA& handle) const {
	if (protoHandle.type == prt::Attributable::PT_BOOL_ARRAY) {
		size_t arraySize = 0;
		const bool* array = attrMap->getBoolArray(protoHandle.key.c_str(), &arraySize);
		setHandleRange(primIndexMap, handle, rangeStart, rangeSize, array, arraySize);
	}
	else {
		size_t arraySize = 0;
		const int32_t* array = attrMap->getIntArray(protoHandle.key.c_str(), &arraySize);
		setHandleRange(primIndexMap, handle, rangeStart, rangeSize, array, arraySize);
	}
}

void ToHoudini::HandleVisitor::operator()(GA_RWHandleSA& handle) const {
	size_t arraySize = 0;
	wchar_t const* const* const array = attrMap->getStringArray(protoHandle.key.c_str(), &arraySize);
	setHandleRange(primIndexMap, handle, rangeStart, rangeSize, array, arraySize);
}

} // namespace AttributeConversion

namespace NameConversion {

std::wstring addStyle(const std::wstring& attrName, const std::wstring& style) {
	return style + STYLE_SEPARATOR + attrName;
}

std::wstring removeStyle(const std::wstring& fullyQualifiedAttrName) {
	const auto p = fullyQualifiedAttrName.find_first_of(STYLE_SEPARATOR);
	if (p != std::string::npos)
		return fullyQualifiedAttrName.substr(p + 1);
	return fullyQualifiedAttrName;
}

std::wstring removeGroups(const std::wstring & fullyQualifiedAttrName) {
	const auto p = fullyQualifiedAttrName.rfind(GROUP_SEPARATOR);
	if (p != std::string::npos)
		return fullyQualifiedAttrName.substr(p + 1);
	return fullyQualifiedAttrName;
}

UT_String toPrimAttr(const std::wstring& fullyQualifiedAttrName) {
	WA("all");

	const auto cv = StringConversionCaches::toPrimAttr.get(fullyQualifiedAttrName);
	if (cv)
		return cv.value();

	std::string s = toOSNarrowFromUTF16(removeStyle(fullyQualifiedAttrName));

	UT_String primAttr(UT_String::ALWAYS_DEEP, s); // ensure owning UT_String inside cache
	StringConversionCaches::toPrimAttr.insert(fullyQualifiedAttrName, primAttr);

	primAttr = UT_VarEncode::encodeAttrib(primAttr);
	return primAttr;
}

std::wstring toRuleAttr(const std::wstring& style, const UT_StringHolder& attrName) {
	WA("all");

	std::string s = attrName.toStdString();

	UT_StringHolder ruleAttr(s);
	ruleAttr = UT_VarEncode::decodeAttrib(ruleAttr);

	return addStyle(toUTF16FromOSNarrow(ruleAttr.toStdString()), style);
}

} // namespace NameConversion
