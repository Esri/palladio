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

std::wstring getNiceName(const std::wstring& fqAttrName) {
	return NameConversion::removeGroups(NameConversion::removeStyle(fqAttrName));
}

} // namespace

std::map<std::wstring, int> getImportOrderMap(const prt::RuleFileInfo* ruleFileInfo) {
	std::map<std::wstring, int> importOrderMap;
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

void setGlobalGroupOrder(RuleAttributeVec& ruleAttributes) {
	AttributeGroupOrder globalGroupOrder;
	for (const auto& attribute : ruleAttributes) {
		for (auto it = std::rbegin(attribute.groups); it != std::rend(attribute.groups); ++it) {
			std::vector<std::wstring> g(it, std::rend(attribute.groups));
			std::reverse(g.begin(), g.end());
			auto ggoIt = globalGroupOrder.emplace(std::make_pair(attribute.ruleFile, g), ORDER_NONE).first;
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

	const std::map<std::wstring, int> importOrderMap = getImportOrderMap(ruleFileInfo);

	std::map<std::wstring, int> nameDuplicateCountMap;

	for (size_t i = 0; i < ruleFileInfo->getNumAttributes(); i++) {
		const prt::RuleFileInfo::Entry* attr = ruleFileInfo->getAttribute(i);

		if (attr->getNumParameters() != 0)
			continue;

		RuleAttribute p;
		p.fqName = attr->getName();
		p.niceName = getNiceName(p.fqName);
		p.mType = attr->getReturnType();

		// TODO: is this correct? import name != rule file name
		std::wstring ruleName = p.fqName;
		size_t idxStyle = ruleName.find(L'$');
		if (idxStyle != std::wstring::npos)
			ruleName = ruleName.substr(idxStyle + 1);
		size_t idxDot = ruleName.find_last_of(L'.');
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

		// no group? put to front
		if (p.groups.empty())
			p.groupOrder = ORDER_FIRST;

		ra.push_back(p);
	}

	setGlobalGroupOrder(ra);
	RuleAttributeSet sortedRuleAttributes(ra.begin(), ra.end());

	return sortedRuleAttributes;
}

bool RuleAttributeCmp::operator()(const RuleAttribute& lhs, const RuleAttribute& rhs) const {
	auto lowerCaseOrdering = [](std::wstring a, std::wstring b) {
		std::transform(a.begin(), a.end(), a.begin(), ::tolower);
		std::transform(b.begin(), b.end(), b.begin(), ::tolower);
		return a < b;
	};

	auto compareRuleFile = [&](const RuleAttribute& a, const RuleAttribute& b) {
		// sort main rule attributes before the rest
		if (a.memberOfStartRuleFile && !b.memberOfStartRuleFile)
			return true;
		if (b.memberOfStartRuleFile && !a.memberOfStartRuleFile)
			return false;

		if (a.ruleOrder != b.ruleOrder)
			return a.ruleOrder < b.ruleOrder;

		return lowerCaseOrdering(a.ruleFile, b.ruleFile);
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

	auto firstDifferentGroupInA = [](const RuleAttribute& a, const RuleAttribute& b) {
		assert(a.groups.size() == b.groups.size());
		size_t i = 0;
		while ((i < a.groups.size()) && (a.groups[i] == b.groups[i])) {
			i++;
		}
		return a.groups[i];
	};

	auto compareGroups = [&](const RuleAttribute& a, const RuleAttribute& b) {
		if (isChildOf(a, b))
			return false; // child a should be sorted after parent b

		if (isChildOf(b, a))
			return true; // child b should be sorted after parent a

		const auto globalOrderA = a.globalGroupOrder;
		const auto globalOrderB = b.globalGroupOrder;
		if (globalOrderA != globalOrderB)
			return (globalOrderA < globalOrderB);

		// sort higher level before lower level
		if (a.groups.size() != b.groups.size())
			return (a.groups.size() < b.groups.size());

		return lowerCaseOrdering(firstDifferentGroupInA(a, b), firstDifferentGroupInA(b, a));
	};

	auto compareAttributeOrder = [&](const RuleAttribute& a, const RuleAttribute& b) {
		if (a.order == b.order)
			return lowerCaseOrdering(a.fqName, b.fqName);

		return a.order < b.order;
	};

	auto attributeOrder = [&](const RuleAttribute& a, const RuleAttribute& b) {
		if (a.ruleFile != b.ruleFile)
			return compareRuleFile(a, b);

		if (a.groups != b.groups)
			return compareGroups(a, b);

		return compareAttributeOrder(a, b);
	};

	return attributeOrder(lhs, rhs);
}
