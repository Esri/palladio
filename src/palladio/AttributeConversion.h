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

#pragma once

#include "LogHandler.h"
#include "PalladioMain.h"
#include "Utils.h"

#include "GU/GU_Detail.h"

// clang-format off
#include "BoostRedirect.h"
#include PLD_BOOST_INCLUDE(/variant.hpp)
// clang-format on

#include <unordered_map>

namespace std {
template <>
struct hash<UT_String> {
	std::size_t operator()(UT_String const& s) const noexcept {
		return s.hash();
	}
};
} // namespace std

namespace AttributeConversion {

class FromHoudini {
private:
	static const bool DBG = true;

public:
	FromHoudini(prt::AttributeMapBuilder& builder) : mBuilder(builder) {}

	bool convert(const GA_ROAttributeRef& ar, const GA_Offset& offset, const std::wstring& name) {
		if (isArrayAttribute(ar))
			return handleArray(ar, offset, name);
		else
			return handleScalar(ar, offset, name);
	}

private:
	bool handleScalar(const GA_ROAttributeRef& ar, const GA_Offset& offset, const std::wstring& name) {
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

	bool handleArray(const GA_ROAttributeRef& ar, const GA_Offset& offset, const std::wstring& name) {
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

	bool isArrayAttribute(const GA_ROAttributeRef& ar) {
		return (ar.getAIFNumericArray() != nullptr) || (ar.getAIFSharedStringArray() != nullptr);
	}

private:
	prt::AttributeMapBuilder& mBuilder;
};

/**
 * attribute type conversion from PRT to Houdini:
 * wstring -> narrow string
 * int32_t -> int32_t
 * bool    -> int8_t
 * double  -> float (single precision!)
 */
using NoHandle = int8_t;
using HandleType = PLD_BOOST_NS::variant<NoHandle, GA_RWBatchHandleS, GA_RWHandleI, GA_RWHandleC, GA_RWHandleF,
                                         GA_RWHandleSA, GA_RWHandleIA, GA_RWHandleDA>;

// bound to life time of PRT attribute map
struct ProtoHandle {
	HandleType handleType;
	std::wstring key;
	prt::AttributeMap::PrimitiveType type; // original PRT type
	size_t cardinality;
};

using HandleMap = std::unordered_map<UT_StringHolder, ProtoHandle>;

enum class ArrayHandling { TUPLE, ARRAY };
void convertAttributes(GU_Detail* detail, HandleMap& handleMap, const prt::AttributeMap* attrMap,
                       const GA_Offset rangeStart, const GA_Size rangeSize,
                       ArrayHandling arrayHandling = ArrayHandling::TUPLE);

} // namespace AttributeConversion

namespace NameConversion {

std::wstring addStyle(const std::wstring& n, const std::wstring& style);
std::wstring removeStyle(const std::wstring& n);
PLD_TEST_EXPORTS_API void separate(const std::wstring& fqName, std::wstring& style, std::wstring& name);

UT_String toPrimAttr(const std::wstring& name);
std::wstring toRuleAttr(const std::wstring& style, const UT_StringHolder& name);

} // namespace NameConversion
