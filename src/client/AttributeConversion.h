#pragma once

#include "utils.h"

#include "GU/GU_Detail.h"

#include "boost/variant.hpp"

#include <unordered_map>


namespace std {
    template<> struct hash<UT_String> {
        std::size_t operator()(UT_String const& s) const noexcept {
            return s.hash();
        }
    };
}


namespace AttributeConversion {

/**
 * attribute type conversion from PRT to Houdini:
 * wstring -> narrow string
 * int32_t -> int32_t
 * bool    -> int8_t
 * double  -> float (single precision!)
 */
using NoHandle   = int8_t;
using HandleType = boost::variant<NoHandle, GA_RWBatchHandleS, GA_RWHandleI, GA_RWHandleC, GA_RWHandleF>;


// bound to life time of PRT attribute map
struct ProtoHandle {
	HandleType handleType;
	std::vector<std::wstring> keys;
	prt::AttributeMap::PrimitiveType type; // original PRT type
};

using HandleMap = std::unordered_map<UT_String, ProtoHandle>;



void extractAttributeNames(HandleMap& handleMap, const prt::AttributeMap* attrMap);
void createAttributeHandles(GU_Detail* detail, HandleMap& handleMap);
void setAttributeValues(HandleMap& handleMap, const prt::AttributeMap* attrMap,
                        const GA_IndexMap& primIndexMap, const GA_Offset rangeStart,
                        const GA_Size rangeSize);

} // namespace AttributeConversion


namespace NameConversion {

UT_String toPrimAttr(const std::string& name);
std::string toRuleAttr(const UT_StringHolder& name);

} // namespace NameConversion
