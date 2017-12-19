#include "AttributeConversion.h"
#include "LogHandler.h"
#include "MultiWatch.h"

#include "boost/algorithm/string.hpp"


namespace {

constexpr bool DBG = false;

class HandleVisitor : public boost::static_visitor<> {
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
			const std::string nv = toOSNarrowFromUTF16(v);
			const UT_StringHolder hv(nv.c_str());
			const GA_Range range(primIndexMap, rangeStart, rangeStart+rangeSize);
			handle.set(range, 0, hv);
			if (DBG) LOG_DBG << "string attr: range = [" << rangeStart << ", " << rangeStart+rangeSize << "): "
			                 << handle.getAttribute()->getName() << " = " << nv;
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
	std::string::size_type dotPos;
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
	const std::string nName = toOSNarrowFromUTF16(handleName);
	const UT_String utName = NameConversion::toPrimAttr(nName);
	if (DBG) LOG_DBG << "handle name conversion: nName = " << nName << ", utName = " << utName;
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
			boost::apply_visitor(hv, h.second.handleType);
		}
	}
}

} // namespace AttributeConversion


namespace {

constexpr const char* ATTR_NAME_TO_HOUDINI[][2] = {
	{ ".", "_dot_" },
	{ "$", "_dollar_" }
};
constexpr size_t ATTR_NAME_TO_HOUDINI_N = sizeof(ATTR_NAME_TO_HOUDINI)/sizeof(ATTR_NAME_TO_HOUDINI[0]);

} // namespace


namespace NameConversion {

// TODO: look into GA_AttributeOptions to transport original prt attribute names between assign and generate node

UT_String toPrimAttr(const std::string& name) {
	std::string s = name;
	for (size_t i = 0; i < ATTR_NAME_TO_HOUDINI_N; i++)
		boost::replace_all(s, ATTR_NAME_TO_HOUDINI[i][0], ATTR_NAME_TO_HOUDINI[i][1]);
	return UT_String(UT_String::ALWAYS_DEEP, s);
}

std::string toRuleAttr(const UT_StringHolder& name) {
	std::string s = name.toStdString();
	for (size_t i = 0; i < ATTR_NAME_TO_HOUDINI_N; i++)
		boost::replace_all(s, ATTR_NAME_TO_HOUDINI[i][1], ATTR_NAME_TO_HOUDINI[i][0]);
	return s;
}

} // namespace NameConversion