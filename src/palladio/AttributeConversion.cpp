/*
 * Copyright 2014-2019 Esri R&D Zurich and VRBN
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

// clang-format off
#include "BoostRedirect.h"
#include PLD_BOOST_INCLUDE(/algorithm/string.hpp)
// clang-format on

#include <mutex>
#include <bitset>

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
			return sh.get();
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
		LOG_DBG << "int array attr: range = [" << start << ", " << start + size
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
		LOG_DBG << "int array attr: range = [" << start << ", " << start + size
		        << "): " << handle.getAttribute()->getName() << " = " << hv;
}

class HandleVisitor : public PLD_BOOST_NS::static_visitor<> {
private:
	const AttributeConversion::ProtoHandle& protoHandle;
	const prt::AttributeMap* attrMap;
	const GA_IndexMap& primIndexMap;
	GA_Offset rangeStart;
	GA_Size rangeSize;

public:
	HandleVisitor(const AttributeConversion::ProtoHandle& ph, const prt::AttributeMap* m, const GA_IndexMap& pim,
	              GA_Offset rStart, GA_Size rSize)
	    : protoHandle(ph), attrMap(m), primIndexMap(pim), rangeStart(rStart), rangeSize(rSize) {}

	void operator()(const AttributeConversion::NoHandle& handle) const {}

	void operator()(GA_RWBatchHandleS& handle) const {
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

	void operator()(GA_RWHandleI& handle) const {
		if (protoHandle.type == prt::Attributable::PT_INT) {
			const int32_t v = attrMap->getInt(protoHandle.key.c_str());
			setHandleRange(primIndexMap, handle, rangeStart, rangeSize, 0, v);
		}
		else if (protoHandle.type == prt::Attributable::PT_INT_ARRAY) {
			LOG_ERR << "int arrays as tuples not yet implemented";
		}
	}

	void operator()(GA_RWHandleC& handle) const {
		if (protoHandle.type == prt::Attributable::PT_BOOL) {
			const bool v = attrMap->getBool(protoHandle.key.c_str());
			setHandleRange(primIndexMap, handle, rangeStart, rangeSize, 0, v);
		}
		else if (protoHandle.type == prt::Attributable::PT_BOOL_ARRAY) {
			LOG_ERR << "bool arrays as tuples not yet implemented";
		}
	}

	void operator()(GA_RWHandleF& handle) const {
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

	void operator()(GA_RWHandleDA& handle) const {
		size_t arraySize = 0;
		const double* const array = attrMap->getFloatArray(protoHandle.key.c_str(), &arraySize);
		setHandleRange(primIndexMap, handle, rangeStart, rangeSize, array, arraySize);
	}

	void operator()(GA_RWHandleIA& handle) const {
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

	void operator()(GA_RWHandleSA& handle) const {
		size_t arraySize = 0;
		wchar_t const* const* const array = attrMap->getStringArray(protoHandle.key.c_str(), &arraySize);
		setHandleRange(primIndexMap, handle, rangeStart, rangeSize, array, arraySize);
	}
};

void addProtoHandle(AttributeConversion::HandleMap& handleMap, const std::wstring& handleName,
                    AttributeConversion::ProtoHandle&& ph) {
	WA("all");

	const UT_StringHolder& utName = NameConversion::toPrimAttr(handleName);
	if (DBG)
		LOG_DBG << "handle name conversion: handleName = " << handleName << ", utName = " << utName;
	handleMap.emplace(utName, std::move(ph));
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

} // namespace

namespace AttributeConversion {

void extractAttributeNames(HandleMap& handleMap, const prt::AttributeMap* attrMap) {
	size_t keyCount = 0;
	wchar_t const* const* keys = attrMap->getKeys(&keyCount);
	for (size_t k = 0; k < keyCount; k++) {
		wchar_t const* key = keys[k];

		ProtoHandle ph;
		ph.type = attrMap->getType(key);
		ph.key.assign(key);
		ph.cardinality = getAttributeCardinality(attrMap, ph.key, ph.type);
		addProtoHandle(handleMap, key, std::move(ph));
	}
}

void createAttributeHandles(GU_Detail* detail, HandleMap& handleMap, bool useArrayTypes) {
	WA("all");

	for (auto& hm : handleMap) {
		const auto& utKey = hm.first;
		const auto& type = hm.second.type;

		HandleType handle; // set to NoHandle by default
		assert(handle.which() == 0);
		switch (type) {
			case prt::Attributable::PT_BOOL:
			case prt::Attributable::PT_BOOL_ARRAY: {
				if (useArrayTypes && (type == prt::Attributable::PT_BOOL_ARRAY)) {
					GA_RWHandleIA h(
					        detail->addIntArray(GA_ATTRIB_PRIMITIVE, utKey, 1, nullptr, nullptr, GA_STORE_INT8));
					if (h.isValid())
						handle = h;
				}
				else {
					GA_RWHandleC h(detail->addIntTuple(GA_ATTRIB_PRIMITIVE, utKey, hm.second.cardinality,
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
					        detail->addFloatArray(GA_ATTRIB_PRIMITIVE, utKey, 1, nullptr, nullptr, GA_STORE_REAL64));
					if (h.isValid())
						handle = h;
				}
				else {
					GA_RWHandleF h(detail->addFloatTuple(GA_ATTRIB_PRIMITIVE, utKey, hm.second.cardinality));
					if (h.isValid())
						handle = h;
				}
				break;
			}
			case prt::Attributable::PT_INT:
			case prt::Attributable::PT_INT_ARRAY: {
				if (useArrayTypes && (type == prt::Attributable::PT_INT_ARRAY)) {
					GA_RWHandleIA h(
					        detail->addIntArray(GA_ATTRIB_PRIMITIVE, utKey, 1, nullptr, nullptr, GA_STORE_INT32));
					if (h.isValid())
						handle = h;
				}
				else {
					GA_RWHandleI h(detail->addIntTuple(GA_ATTRIB_PRIMITIVE, utKey, hm.second.cardinality));
					if (h.isValid())
						handle = h;
				}
				break;
			}
			case prt::Attributable::PT_STRING:
			case prt::Attributable::PT_STRING_ARRAY: {
				if (useArrayTypes && (type == prt::Attributable::PT_STRING_ARRAY)) {
					GA_RWHandleSA h(
					        detail->addStringArray(GA_ATTRIB_PRIMITIVE, utKey, 1, nullptr, nullptr, GA_STORE_STRING));
					if (h.isValid())
						handle = h;
				}
				else {
					GA_RWBatchHandleS h(detail->addStringTuple(GA_ATTRIB_PRIMITIVE, utKey, hm.second.cardinality));
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

		if (handle.which() != 0) {
			hm.second.handleType = handle;
			if (DBG)
				LOG_DBG << "added attr handle " << utKey << " of type " << handle.type().name();
		}
		else if (DBG)
			LOG_DBG << "could not update handle for primitive attribute " << utKey;
	}
}

void setAttributeValues(HandleMap& handleMap, const prt::AttributeMap* attrMap, const GA_IndexMap& primIndexMap,
                        const GA_Offset rangeStart, const GA_Size rangeSize) {
	for (auto& h : handleMap) {
		if (attrMap->hasKey(h.second.key.c_str())) {
			const HandleVisitor hv(h.second, attrMap, primIndexMap, rangeStart, rangeSize);
			PLD_BOOST_NS::apply_visitor(hv, h.second.handleType);
		}
	}
}

void convertAttributes(GU_Detail* detail, HandleMap& handleMap, const prt::AttributeMap* attrMap,
                       const GA_Offset rangeStart, const GA_Size rangeSize, ArrayHandling arrayHandling) {
	const GA_IndexMap& primIndexMap = detail->getIndexMap(GA_ATTRIB_PRIMITIVE);
	extractAttributeNames(handleMap, attrMap);
	createAttributeHandles(detail, handleMap, arrayHandling == ArrayHandling::ARRAY);
	setAttributeValues(handleMap, attrMap, primIndexMap, rangeStart, rangeSize);
}

} // namespace AttributeConversion

namespace {

constexpr const char* RULE_ATTR_NAME_TO_PRIM_ATTR[][2] = {{".", "__"}};
constexpr size_t RULE_ATTR_NAME_TO_PRIM_ATTR_N =
        sizeof(RULE_ATTR_NAME_TO_PRIM_ATTR) / sizeof(RULE_ATTR_NAME_TO_PRIM_ATTR[0]);

constexpr wchar_t STYLE_SEPARATOR = L'$';

} // namespace

namespace NameConversion {

std::wstring addStyle(const std::wstring& n, const std::wstring& style) {
	return style + STYLE_SEPARATOR + n;
}

std::wstring removeStyle(const std::wstring& n) {
	const auto p = n.find_first_of(STYLE_SEPARATOR);
	if (p != std::string::npos)
		return n.substr(p + 1);
	return n;
}

UT_String toPrimAttr(const std::wstring& name) {
	WA("all");

	const auto cv = StringConversionCaches::toPrimAttr.get(name);
	if (cv)
		return cv.get();

	std::string s = toOSNarrowFromUTF16(removeStyle(name));
	for (size_t i = 0; i < RULE_ATTR_NAME_TO_PRIM_ATTR_N; i++)
		PLD_BOOST_NS::replace_all(s, RULE_ATTR_NAME_TO_PRIM_ATTR[i][0], RULE_ATTR_NAME_TO_PRIM_ATTR[i][1]);

	UT_String primAttr(UT_String::ALWAYS_DEEP, s); // ensure owning UT_String inside cache
	StringConversionCaches::toPrimAttr.insert(name, primAttr);
	return primAttr;
}

std::wstring toRuleAttr(const std::wstring& style, const UT_StringHolder& name) {
	WA("all");

	std::string s = name.toStdString();
	for (size_t i = 0; i < RULE_ATTR_NAME_TO_PRIM_ATTR_N; i++)
		PLD_BOOST_NS::replace_all(s, RULE_ATTR_NAME_TO_PRIM_ATTR[i][1], RULE_ATTR_NAME_TO_PRIM_ATTR[i][0]);
	return addStyle(toUTF16FromOSNarrow(s), style);
}

void separate(const std::wstring& fqName, std::wstring& style, std::wstring& name) {
	if (fqName.length() <= 1)
		return;

	const auto p = fqName.find_first_of(STYLE_SEPARATOR);
	if (p == std::wstring::npos) {
		name.assign(fqName);
	}
	else if (p > 0 && p < fqName.length() - 1) {
		style.assign(fqName.substr(0, p));
		name.assign(fqName.substr(p + 1));
	}
	else if (p == 0) { // empty style
		name = fqName.substr(1);
	}
	else if (p == fqName.length() - 1) { // empty name
		style = fqName.substr(0, p);
	}
}

} // namespace NameConversion