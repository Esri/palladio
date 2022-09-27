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

#include "RuleAttributes.h"
#include "AttributeConversion.h"

#include "prt/Annotation.h"
#include "prt/RuleFileInfo.h"

#include <algorithm>
#include <cassert>
#include <filesystem>
#include <ostream>
#include <string>
#include <vector>
#include <string>

namespace {

using ImportOrderMap = std::map<std::wstring, size_t>;

std::wstring getNiceName(const std::wstring& fqAttrName) {
	return NameConversion::removeGroups(NameConversion::removeStyle(fqAttrName));
}

} // namespace

ImportOrderMap getImportOrderMap(const prt::RuleFileInfo* ruleFileInfo) {
	ImportOrderMap importOrderMap;
	int importOrder = 0;
	for (size_t i = 0; i < ruleFileInfo->getNumAnnotations(); i++) {
		const prt::Annotation* an = ruleFileInfo->getAnnotation(i);
		const wchar_t* anName = an->getName();
		if (std::wcscmp(anName, ANNOT_IMPORTS) == 0) {
			for (int argIdx = 0; argIdx < an->getNumArguments(); argIdx++) {
				const prt::AnnotationArgument* anArg = an->getArgument(argIdx);
				if (anArg->getType() == prt::AAT_STR) {
					const wchar_t* anKey = anArg->getKey();
					if (std::wcscmp(anKey, ANNOT_IMPORTS_KEY) == 0) {
						const wchar_t* importRuleCharPtr = anArg->getStr();
						if (importRuleCharPtr != nullptr) {
							std::wstring importRule = importRuleCharPtr;
							importOrderMap[importRule] = importOrder++;
						}
					}
				}
			}
		}
	}
	return importOrderMap;
}

// maps the highest attribute order from all attributes within a group to its group string key
void setGlobalGroupOrder(RuleAttributeVec& ruleAttributes) {
	AttributeGroupOrder globalGroupOrder;
	for (const auto& attribute : ruleAttributes) {
		// go over all parent group vector and assign the group order
		for (auto it = std::rbegin(attribute.groups); it != std::rend(attribute.groups); ++it) {
			std::vector<std::wstring> groupVec(it, std::rend(attribute.groups));
			std::reverse(groupVec.begin(), groupVec.end());
			auto ggoIt = globalGroupOrder.emplace(std::make_pair(attribute.ruleFile, groupVec), ORDER_NONE).first;
			ggoIt->second = std::min(attribute.groupOrder, ggoIt->second);
		}
	}

	for (auto& attribute : ruleAttributes) {
		const auto it = globalGroupOrder.find(std::make_pair(attribute.ruleFile, attribute.groups));
		attribute.globalGroupOrder = (it != globalGroupOrder.end()) ? it->second : ORDER_NONE;
	}
}

RuleAttributeSet getRuleAttributes(const std::wstring& ruleFile, const prt::RuleFileInfo* ruleFileInfo) {
	RuleAttributeVec ra;

	std::wstring mainCgaRuleName = std::filesystem::path(ruleFile).stem().wstring();

	const ImportOrderMap importOrderMap = getImportOrderMap(ruleFileInfo);

	std::map<std::wstring, int> nameDuplicateCountMap;

	for (size_t i = 0; i < ruleFileInfo->getNumAttributes(); i++) {
		const prt::RuleFileInfo::Entry* attr = ruleFileInfo->getAttribute(i);

		if (attr->getNumParameters() != 0)
			continue;

		RuleAttribute p;
		p.fqName = attr->getName();
		p.niceName = getNiceName(p.fqName);
		p.mType = attr->getReturnType();

		std::wstring ruleName = p.fqName;
		size_t idxStyle = ruleName.find(NameConversion::STYLE_SEPARATOR);
		if (idxStyle != std::wstring::npos)
			ruleName = ruleName.substr(idxStyle + 1);
		size_t idxDot = ruleName.find_last_of(NameConversion::GROUP_SEPARATOR);
		if (idxDot != std::wstring::npos) {
			p.ruleFile = ruleName.substr(0, idxDot);
		}
		else {
			p.ruleFile = mainCgaRuleName;
			p.memberOfStartRuleFile = true;
		}

		const auto importOrder = importOrderMap.find(p.ruleFile);
		p.ruleOrder = (importOrder != importOrderMap.end()) ? importOrder->second : ORDER_NONE;

		bool hidden = false;
		for (size_t a = 0; a < attr->getNumAnnotations(); a++) {
			const prt::Annotation* an = attr->getAnnotation(a);
			const wchar_t* anName = an->getName();
			if (std::wcscmp(anName, ANNOT_HIDDEN) == 0)
				hidden = true;
			else if (std::wcscmp(anName, ANNOT_ORDER) == 0) {
				if (an->getNumArguments() >= 1 && an->getArgument(0)->getType() == prt::AAT_FLOAT) {
					p.order = static_cast<int>(an->getArgument(0)->getFloat());
				}
			}
			else if (std::wcscmp(anName, ANNOT_GROUP) == 0) {
				for (int argIdx = 0; argIdx < an->getNumArguments(); argIdx++) {
					if (an->getArgument(argIdx)->getType() == prt::AAT_STR) {
						p.groups.push_back(an->getArgument(argIdx)->getStr());
					}
					else if (argIdx == an->getNumArguments() - 1 &&
					         an->getArgument(argIdx)->getType() == prt::AAT_FLOAT) {
						p.groupOrder = static_cast<int>(an->getArgument(argIdx)->getFloat());
					}
				}
			}
		}
		if (hidden)
			continue;

		ra.push_back(p);
	}

	setGlobalGroupOrder(ra);
	RuleAttributeSet sortedRuleAttributes(ra.begin(), ra.end());

	return sortedRuleAttributes;
}

bool RuleAttributeCmp::operator()(const RuleAttribute& lhs, const RuleAttribute& rhs) const {

	auto compareRuleFile = [](const RuleAttribute& a, const RuleAttribute& b) {
		// sort main rule attributes before the rest
		if (a.memberOfStartRuleFile && !b.memberOfStartRuleFile)
			return true;
		if (b.memberOfStartRuleFile && !a.memberOfStartRuleFile)
			return false;

		if (a.ruleOrder != b.ruleOrder)
			return a.ruleOrder < b.ruleOrder;

		return a.ruleFile < b.ruleFile;
	};

	auto isChildOf = [](const RuleAttribute& child, const RuleAttribute& parent) {
		const size_t np = parent.groups.size();
		const size_t nc = child.groups.size();

		// parent path must be shorter
		if (np >= nc)
			return false;

		// parent and child paths must be identical
		for (size_t i = 0; i < np; i++) {
			if (parent.groups[i] != child.groups[i])
				return false;
		}

		return true;
	};

	auto compareGroups = [](const RuleAttribute& a, const RuleAttribute& b) {
		const size_t groupSizeA = a.groups.size();
		const size_t groupSizeB = b.groups.size();

		for (size_t i = 0; i < std::max(groupSizeA, groupSizeB); ++i) {
			// a descendant of b
			if (i >= groupSizeA)
				return false;

			// b descendant of a
			if (i >= groupSizeB)
				return true;

			// difference in groups
			if (a.groups[i] != b.groups[i])
				return a.groups[i] < b.groups[i];
		}
		return false;
	};

	auto compareOrderToGroupOrder = [](const RuleAttribute& ruleAttrWithGroups,
	                                    const RuleAttribute& ruleAttrWithoutGroups) {
		if ((ruleAttrWithGroups.groups.size() > 0) &&
		    (ruleAttrWithGroups.globalGroupOrder == ruleAttrWithoutGroups.order))
			return ruleAttrWithGroups.groups[0] <= ruleAttrWithoutGroups.niceName;

		return ruleAttrWithGroups.globalGroupOrder < ruleAttrWithoutGroups.order;
	};

	auto compareGroupOrder = [&](const RuleAttribute& a, const RuleAttribute& b) {
		if (b.groups.empty())
			return compareOrderToGroupOrder(a, b);

		if (a.groups.empty())
			return !compareOrderToGroupOrder(b, a);

		if (isChildOf(a, b))
			return false; // child a should be sorted after parent b

		if (isChildOf(b, a))
			return true; // child b should be sorted after parent a

		const auto globalOrderA = a.globalGroupOrder;
		const auto globalOrderB = b.globalGroupOrder;
		if (globalOrderA != globalOrderB)
			return (globalOrderA < globalOrderB);

		return compareGroups(a, b);
	};

	auto compareAttributeOrder = [](const RuleAttribute& a, const RuleAttribute& b) {
		if (a.order == b.order)
			return a.niceName < b.niceName;

		return a.order < b.order;
	};

	auto attributeOrder = [&](const RuleAttribute& a, const RuleAttribute& b) {
		if (a.ruleFile != b.ruleFile)
			return compareRuleFile(a, b);

		if (a.groups != b.groups)
			return compareGroupOrder(a, b);

		return compareAttributeOrder(a, b);
	};

	return attributeOrder(lhs, rhs);
}
