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

#pragma once

#include "PalladioMain.h"
#include "Utils.h"

#include "prt/AttributeMap.h"

#include "GU/GU_Detail.h"

#include <unordered_map>
#include <variant>
#include <string>

namespace std {
template <>
struct hash<UT_String> {
	std::size_t operator()(UT_String const& s) const noexcept {
		return s.hash();
	}
};
} // namespace std

namespace AttributeConversion {

/**
 * attribute type conversion between PRT and Houdini:
 * wstring <-> narrow string
 * int32_t <-> int32_t
 * bool    <-> int8_t
 * double  <-> double
 */

class FromHoudini {
public:
	FromHoudini(prt::AttributeMapBuilder& builder) : mBuilder(builder) {}
	FromHoudini(const FromHoudini&) = delete;
	FromHoudini(FromHoudini&&) = delete;
	FromHoudini& operator=(const FromHoudini&) = delete;
	FromHoudini& operator=(FromHoudini&&) = delete;
	virtual ~FromHoudini() = default;

	bool convert(const GA_ROAttributeRef& ar, const GA_Offset& offset, const std::wstring& name);

private:
	bool handleScalar(const GA_ROAttributeRef& ar, const GA_Offset& offset, const std::wstring& name);
	bool handleArray(const GA_ROAttributeRef& ar, const GA_Offset& offset, const std::wstring& name);

private:
	prt::AttributeMapBuilder& mBuilder;
};

class ToHoudini {
public:
	ToHoudini(GU_Detail* detail) : mDetail(detail) {}
	ToHoudini(const ToHoudini&) = delete;
	ToHoudini(ToHoudini&&) = delete;
	ToHoudini& operator=(const ToHoudini&) = delete;
	ToHoudini& operator=(ToHoudini&&) = delete;
	virtual ~ToHoudini() = default;

	enum class ArrayHandling { TUPLE, ARRAY };
	void convert(const prt::AttributeMap* attrMap, const GA_Offset rangeStart, const GA_Size rangeSize,
	             ArrayHandling arrayHandling = ArrayHandling::TUPLE);

private:
	using NoHandle = int8_t;
	using HandleType = std::variant<NoHandle, GA_RWBatchHandleS, GA_RWHandleI, GA_RWHandleC, GA_RWHandleF,
	                                         GA_RWHandleSA, GA_RWHandleIA, GA_RWHandleDA>;

	struct ProtoHandle {
		HandleType handleType;
		std::wstring key;
		prt::AttributeMap::PrimitiveType type; // original PRT type
		size_t cardinality;
	};

	class HandleVisitor {
	public:
		HandleVisitor(const ProtoHandle& ph, const prt::AttributeMap* m, const GA_IndexMap& pim, GA_Offset rStart,
		              GA_Size rSize)
		    : protoHandle(ph), attrMap(m), primIndexMap(pim), rangeStart(rStart), rangeSize(rSize) {}
		void operator()(const NoHandle& handle) const {}
		void operator()(GA_RWBatchHandleS& handle) const;
		void operator()(GA_RWHandleI& handle) const;
		void operator()(GA_RWHandleC& handle) const;
		void operator()(GA_RWHandleF& handle) const;
		void operator()(GA_RWHandleDA& handle) const;
		void operator()(GA_RWHandleIA& handle) const;
		void operator()(GA_RWHandleSA& handle) const;

	private:
		const ProtoHandle& protoHandle;
		const prt::AttributeMap* attrMap;
		const GA_IndexMap& primIndexMap;
		GA_Offset rangeStart;
		GA_Size rangeSize;
	};

	using HandleMap = std::unordered_map<UT_StringHolder, ProtoHandle>;

	void extractAttributeNames(const prt::AttributeMap* attrMap);
	void createAttributeHandles(bool useArrayTypes);
	void setAttributeValues(const prt::AttributeMap* attrMap, const GA_IndexMap& primIndexMap,
	                        const GA_Offset rangeStart, const GA_Size rangeSize);
	void addProtoHandle(HandleMap& handleMap, const std::wstring& handleName, ProtoHandle&& ph);

private:
	GU_Detail* mDetail;
	HandleMap mHandleMap;
};

} // namespace AttributeConversion

namespace NameConversion {

constexpr wchar_t STYLE_SEPARATOR = L'$';
constexpr wchar_t GROUP_SEPARATOR = L'.';

std::wstring addStyle(const std::wstring& attrName, const std::wstring& style);
std::wstring removeGroups(const std::wstring & fullyQuantifiedAttrName);
std::wstring removeStyle(const std::wstring& fullyQuantifiedAttrName);
PLD_TEST_EXPORTS_API void separate(const std::wstring& fullyQuantifiedAttrName, std::wstring& style,
                                   std::wstring& attrName);

UT_String toPrimAttr(const std::wstring& fullyQuantifiedAttrName);
std::wstring toRuleAttr(const std::wstring& style, const UT_StringHolder& attrName);

} // namespace NameConversion
