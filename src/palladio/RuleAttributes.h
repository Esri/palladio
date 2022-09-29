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

#include "prt/Annotation.h"

#include <filesystem>
#include <limits>
#include <map>
#include <set>
#include <string>
#include <vector>


namespace prt {
class RuleFileInfo;
}

constexpr const wchar_t* ANNOT_RANGE = L"@Range";
constexpr const wchar_t* ANNOT_ANGLE = L"@Angle";
constexpr const wchar_t* ANNOT_PERCENT = L"@Percent";
constexpr const wchar_t* ANNOT_ENUM = L"@Enum";
constexpr const wchar_t* ANNOT_HIDDEN = L"@Hidden";
constexpr const wchar_t* ANNOT_COLOR = L"@Color";
constexpr const wchar_t* ANNOT_DIR = L"@Directory";
constexpr const wchar_t* ANNOT_FILE = L"@File";
constexpr const wchar_t* ANNOT_DESCRIPTION = L"@Description";
constexpr const wchar_t* ANNOT_ORDER = L"@Order";
constexpr const wchar_t* ANNOT_GROUP = L"@Group";
constexpr const wchar_t* ANNOT_IMPORTS = L"_$IMPORTS";
constexpr const wchar_t* ANNOT_IMPORTS_KEY = L"fullPrefix";

constexpr int ORDER_FIRST = std::numeric_limits<int>::min();
constexpr int ORDER_NONE = std::numeric_limits<int>::max();

using AttributeGroup = std::vector<std::wstring>;
using AttributeGroupOrder = std::map<std::pair<std::wstring, AttributeGroup>, int>;

struct RuleAttribute {
	std::wstring fqName;    // fully qualified rule name (i.e. including style prefix)
	std::wstring niceName;  // rule name without style and groups
	prt::AnnotationArgumentType mType;

	AttributeGroup groups; // groups can be nested
	int order = ORDER_NONE;
	int groupOrder = ORDER_NONE;
	int globalGroupOrder = ORDER_NONE;

	std::wstring ruleFile;
	int ruleOrder = ORDER_NONE;
	bool memberOfStartRuleFile = false;
};

struct RuleAttributeCmp {
	bool operator()(const RuleAttribute& lhs, const RuleAttribute& rhs) const;
};

using RuleAttributeVec = std::vector<RuleAttribute>;
using RuleAttributeSet = std::set<RuleAttribute, RuleAttributeCmp>;
using RuleAttributeMap = std::map<std::wstring, RuleAttribute>;

RuleAttributeSet getRuleAttributes(const std::filesystem::path& ruleFile, const prt::RuleFileInfo* ruleFileInfo);
void setGlobalGroupOrder(RuleAttributeVec& ruleAttributes);