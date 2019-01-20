/*
 * Copyright 2014-2018 Esri R&D Zurich and VRBN
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

#include "BoostRedirect.h"
#include PLD_BOOST_INCLUDE(/algorithm/string.hpp)

#include <mutex>
#include <bitset>


namespace {

constexpr bool DBG = false;

namespace StringConversionCaches {
	LockedLRUCache<std::wstring, UT_String> toPrimAttr(1 << 12);
}

class HandleVisitor : public PLD_BOOST_NS::static_visitor<> {
private:
	const AttributeConversion::ProtoHandle& protoHandle;
	const prt::AttributeMap*                attrMap;
	const GA_IndexMap&                      primIndexMap;
	GA_Offset                               rangeStart;
	GA_Size                                 rangeSize;

public:
	HandleVisitor(const AttributeConversion::ProtoHandle& ph, const prt::AttributeMap* m,
	              const GA_IndexMap& pim, GA_Offset rStart, GA_Size rSize)
		: protoHandle(ph), attrMap(m), primIndexMap(pim), rangeStart(rStart), rangeSize(rSize) { }

    void operator()(const AttributeConversion::NoHandle& handle) const { }

    void operator()(GA_RWBatchHandleS& handle) const {
		assert(protoHandle.keys.size() == handle.getTupleSize());
	    assert(protoHandle.keys.size() == 1);
	    const wchar_t* v = attrMap->getString(protoHandle.keys.front().c_str());
		if (v && std::wcslen(v) > 0) {

			const UT_String hv = [&v]() {
				const auto sh = StringConversionCaches::toPrimAttr.get(v);
				if (sh)
					return sh.get();
				const std::string nv = toOSNarrowFromUTF16(v);
				UT_String hv(UT_String::ALWAYS_DEEP, nv); // ensure owning UT_String inside cache
				StringConversionCaches::toPrimAttr.insert(v, hv);
				return hv;
			}();

			const GA_Range range(primIndexMap, rangeStart, rangeStart+rangeSize);
			handle.set(range, 0, hv);
			if (DBG) LOG_DBG << "string attr: range = [" << rangeStart << ", " << rangeStart+rangeSize << "): "
			                 << handle.getAttribute()->getName() << " = " << hv;
		}
    }

    void operator()(const GA_RWHandleI& handle) const {
		assert(protoHandle.keys.size() == handle.getTupleSize());
		assert(protoHandle.keys.size() == 1);
		const int32_t v = attrMap->getInt(protoHandle.keys.front().c_str());
	    handle.setBlock(rangeStart, rangeSize, &v, 0, 0);
		if (DBG) LOG_DBG << "int attr: range = [" << rangeStart << ", " << rangeStart+rangeSize << "): "
		                 << handle.getAttribute()->getName() << " = " << v;
    }

    void operator()(const GA_RWHandleC& handle) const {
		constexpr int8_t valFalse = 0;
		constexpr int8_t valTrue  = 1;

		assert(protoHandle.keys.size() == handle.getTupleSize());
		assert(protoHandle.keys.size() == 1);
	    const bool v = attrMap->getBool(protoHandle.keys.front().c_str());
	    const int8_t hv = v ? valTrue : valFalse;
	    handle.setBlock(rangeStart, rangeSize, &hv, 0, 0);
		if (DBG) LOG_DBG << "bool attr: range = [" << rangeStart << ", " << rangeStart+rangeSize << "): "
		                 << handle.getAttribute()->getName() << " = " << v;
    }

    void operator()(const GA_RWHandleF& handle) const {
		assert(protoHandle.keys.size() == handle.getTupleSize());
		for (size_t c = 0; c < protoHandle.keys.size(); c++) {
	        const auto v = attrMap->getFloat(protoHandle.keys[c].c_str());
			const auto hv = static_cast<fpreal32>(v);
		    handle.setBlock(rangeStart, rangeSize, &hv, 0, c); // using stride = 0 to always set the same value
		    if (DBG) LOG_DBG << "float attr: component = " << c
		                     << ", range = [" << rangeStart << ", " << rangeStart+rangeSize << "): "
			                 << handle.getAttribute()->getName() << " = " << v;
		}
	}
};

bool hasKeys(const prt::AttributeMap* attrMap, const std::vector<std::wstring>& keys) {
	for (const auto& k: keys)
		if (!attrMap->hasKey(k.c_str()))
			return false;
	return true;
}

struct Candidate {
	std::vector<std::wstring> keys;
	std::string::size_type dotPos = -1;
};

using Candidates = std::map<std::wstring, Candidate>;

/**
 * specific test for float attributes which match the "foo.{r,g,b}" pattern (for color)
 */
bool isColorGroup(const std::wstring& primary, const Candidate& candidate, const prt::AttributeMap* attrMap) {
	WA("all");

	const auto& keys = candidate.keys;
	if ((candidate.dotPos == std::wstring::npos) || keys.size() != 3)
		return false;

	const auto firstType = attrMap->getType(keys.front().c_str());
	if (firstType != prt::AttributeMap::PT_FLOAT)
		return false;

	std::bitset<3> flags; // r g b found
	for (const auto& k: keys) {
		const auto t = attrMap->getType(k.c_str());
		if (t != firstType)
			return false;

		const std::wstring s = k.substr(candidate.dotPos+1);
		if (s == L"r") flags[0] = true;
		if (s == L"g") flags[1] = true;
		if (s == L"b") flags[2] = true;
	}

	return flags.all();
}

void addProtoHandle(AttributeConversion::HandleMap& handleMap, const std::wstring& handleName,
                    AttributeConversion::ProtoHandle&& ph)
{
	WA("all");

	const UT_StringHolder& utName = NameConversion::toPrimAttr(handleName);
	if (DBG) LOG_DBG << "handle name conversion: handleName = " << handleName << ", utName = " << utName;
	handleMap.emplace(utName, std::move(ph));
}

} // namespace


namespace AttributeConversion {

void extractAttributeNames(HandleMap& handleMap, const prt::AttributeMap* attrMap) {
	WA("all");

	Candidates candidates;

	// split keys with a dot
	{
		WA("split");

		size_t keyCount = 0;
		wchar_t const* const* keys = attrMap->getKeys(&keyCount);
		for (size_t k = 0; k < keyCount; k++) {
			std::wstring key(keys[k]);

			auto p = key.find_first_of(L'.');

			std::wstring primary;
			if (p != std::wstring::npos)
				primary = key.substr(0, p);
			else
				primary = key;

			auto r = candidates.emplace(primary, Candidate());
			Candidate& c = r.first->second;
			c.keys.emplace_back(std::move(key));
			c.dotPos = p;
		}
	}

	// detect keys which match the "foo.{r,g,b}" pattern
	{
		WA("detect/add attr groups");

		for (auto& c: candidates) {
			const auto& primary = c.first;
			auto& candidate = c.second;

			const bool groupable = isColorGroup(primary, candidate, attrMap);
			if (groupable) {
				WA("add color groups");

				ProtoHandle ph;
				ph.type = attrMap->getType(candidate.keys.front().c_str());

				// naive way to get the r,g,b order
				std::sort(candidate.keys.begin(), candidate.keys.end());
				std::reverse(candidate.keys.begin(), candidate.keys.end());
				ph.keys = std::move(candidate.keys);

				addProtoHandle(handleMap, primary, std::move(ph));
			}
			else {
				WA("add individual attrs");

				for (const auto& key: candidate.keys) {
					ProtoHandle ph;
					ph.type = attrMap->getType(key.c_str());
					ph.keys.emplace_back(key);
					addProtoHandle(handleMap, key, std::move(ph));
				}
			}
		}
	}
}

void createAttributeHandles(GU_Detail* detail, HandleMap& handleMap) {
	WA("all");

	for (auto& hm: handleMap) {
		const auto& utKey = hm.first;
		const auto& type = hm.second.type;
		const size_t tupleSize = hm.second.keys.size();

		HandleType handle; // set to NoHandle by default
		assert(handle.which() == 0);
		switch (type) {
			case prt::Attributable::PT_BOOL: {
				GA_RWHandleC h(detail->addIntTuple(GA_ATTRIB_PRIMITIVE, utKey, tupleSize, GA_Defaults(0),
				                                   nullptr, nullptr, GA_STORE_INT8));
				if (h.isValid())
					handle = h;
				break;
			}
			case prt::Attributable::PT_FLOAT: {
				GA_RWHandleF h(detail->addFloatTuple(GA_ATTRIB_PRIMITIVE, utKey, tupleSize));
				if (h.isValid())
					handle = h;
				break;
			}
			case prt::Attributable::PT_INT: {
				GA_RWHandleI h(detail->addIntTuple(GA_ATTRIB_PRIMITIVE, utKey, tupleSize));
				if (h.isValid())
					handle = h;
				break;
			}
			case prt::Attributable::PT_STRING: {
				GA_RWBatchHandleS h(detail->addStringTuple(GA_ATTRIB_PRIMITIVE, utKey, tupleSize));
				if (h.isValid())
					handle = h;
				break;
			}

			// TODO: add support for array attributes

			default:
				if (DBG) LOG_DBG << "ignored: " << utKey;
				break;
		}

		if (handle.which() != 0) {
			hm.second.handleType = handle;
			if (DBG) LOG_DBG << "added attr handle " << utKey << " of type " << handle.type().name();
		}
		else if (DBG) LOG_DBG << "could not update handle for primitive attribute " << utKey;
	}
}

void setAttributeValues(HandleMap& handleMap, const prt::AttributeMap* attrMap,
                        const GA_IndexMap& primIndexMap, const GA_Offset rangeStart, const GA_Size rangeSize)
{
	WA("all");

	for (auto& h: handleMap) {
		if (hasKeys(attrMap, h.second.keys)) {
			const HandleVisitor hv(h.second, attrMap, primIndexMap, rangeStart, rangeSize);
			PLD_BOOST_NS::apply_visitor(hv, h.second.handleType);
		}
	}
}

} // namespace AttributeConversion


namespace {

constexpr const char* RULE_ATTR_NAME_TO_PRIM_ATTR[][2] = {
	{ ".", "__" }
};
constexpr size_t RULE_ATTR_NAME_TO_PRIM_ATTR_N = sizeof(RULE_ATTR_NAME_TO_PRIM_ATTR)/sizeof(RULE_ATTR_NAME_TO_PRIM_ATTR[0]);

constexpr wchar_t STYLE_SEPARATOR = L'$';

} // namespace


namespace NameConversion {

std::wstring addStyle(const std::wstring& n, const std::wstring& style) {
	return style + STYLE_SEPARATOR + n;
}

std::wstring removeStyle(const std::wstring& n) {
	const auto p = n.find_first_of(STYLE_SEPARATOR);
	if (p != std::string::npos)
		return n.substr(p+1);
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
	else if (p > 0 && p < fqName.length()-1) {
		style.assign(fqName.substr(0,p));
		name.assign(fqName.substr(p + 1));
	}
	else if (p == 0) { // empty style
		name = fqName.substr(1);
	}
	else if (p == fqName.length()-1) { // empty name
		style = fqName.substr(0, p);
	}
}

} // namespace NameConversion