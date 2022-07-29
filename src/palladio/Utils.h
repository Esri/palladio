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

#include "prt/AttributeMap.h"
#include "prt/EncoderInfo.h"
#include "prt/InitialShape.h"
#include "prt/Object.h"
#include "prt/OcclusionSet.h"
#include "prt/ResolveMap.h"
#include "prt/RuleFileInfo.h"

#include "GA/GA_Primitive.h"

#include <memory>
#include <string>
#include <vector>
#include <filesystem>

struct PRTDestroyer {
	void operator()(prt::Object const* p) {
		if (p)
			p->destroy();
	}
};

using PrimitiveNOPtrVector = std::vector<const GA_Primitive*>;

using ObjectUPtr = std::unique_ptr<const prt::Object, PRTDestroyer>;
using InitialShapeNOPtrVector = std::vector<const prt::InitialShape*>;
using AttributeMapNOPtrVector = std::vector<const prt::AttributeMap*>;
using CacheObjectUPtr = std::unique_ptr<prt::CacheObject, PRTDestroyer>;
using AttributeMapUPtr = std::unique_ptr<const prt::AttributeMap, PRTDestroyer>;
using AttributeMapVector = std::vector<AttributeMapUPtr>;
using AttributeMapBuilderUPtr = std::unique_ptr<prt::AttributeMapBuilder, PRTDestroyer>;
using AttributeMapBuilderVector = std::vector<AttributeMapBuilderUPtr>;
using InitialShapeBuilderUPtr = std::unique_ptr<prt::InitialShapeBuilder, PRTDestroyer>;
using InitialShapeBuilderVector = std::vector<InitialShapeBuilderUPtr>;
using ResolveMapSPtr = std::shared_ptr<const prt::ResolveMap>;
using ResolveMapUPtr = std::unique_ptr<const prt::ResolveMap, PRTDestroyer>;
using ResolveMapBuilderUPtr = std::unique_ptr<prt::ResolveMapBuilder, PRTDestroyer>;
using RuleFileInfoUPtr = std::unique_ptr<const prt::RuleFileInfo, PRTDestroyer>;
using EncoderInfoUPtr = std::unique_ptr<const prt::EncoderInfo, PRTDestroyer>;
using OcclusionSetUPtr = std::unique_ptr<prt::OcclusionSet, PRTDestroyer>;

PLD_TEST_EXPORTS_API std::vector<std::wstring> tokenizeStringToVector(std::wstring commaSeparatedString,
                                                                      wchar_t delimiter);

PLD_TEST_EXPORTS_API void getCGBs(const ResolveMapSPtr& rm, std::vector<std::pair<std::wstring, std::wstring>>& cgbs);
PLD_TEST_EXPORTS_API const prt::AttributeMap* createValidatedOptions(const wchar_t* encID,
                                                                     const prt::AttributeMap* unvalidatedOptions);
PLD_TEST_EXPORTS_API std::string objectToXML(prt::Object const* obj);

void getLibraryPath(std::filesystem::path& path, const void* func);
std::string getSharedLibraryPrefix();
std::string getSharedLibrarySuffix();

PLD_TEST_EXPORTS_API std::string toOSNarrowFromUTF16(const std::wstring& osWString);
std::wstring toUTF16FromOSNarrow(const std::string& osString);
std::string toUTF8FromOSNarrow(const std::string& osString);

PLD_TEST_EXPORTS_API std::wstring toFileURI(const std::filesystem::path& p);
std::wstring toFileURI(const std::string& p);
PLD_TEST_EXPORTS_API std::wstring percentEncode(const std::string& utf8String);

// hash_combine function from boost library: https://www.boost.org/doc/libs/1_73_0/boost/container_hash/hash.hpp
template <class SizeT>
inline void hash_combine(SizeT& seed, SizeT value) {
	seed ^= value + 0x9e3779b9 + (seed << 6) + (seed >> 2);
}

inline void replace_all_not_of(std::wstring& s, const std::wstring& allowedChars) {
	std::wstring::size_type pos = 0;
	while (pos < s.size()) {
		pos = s.find_first_not_of(allowedChars, pos);
		if (pos == std::wstring::npos)
			break;
		s[pos++] = L'_';
	}
}

inline bool startsWithAnyOf(const std::string& s, const std::vector<std::string>& sv) {
	for (const auto& v : sv) {
		if (s.find(v) == 0)
			return true;
	}
	return false;
}
