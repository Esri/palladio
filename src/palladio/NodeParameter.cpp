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

#include "NodeParameter.h"
#include "AttributeConversion.h"
#include "LogHandler.h"
#include "SOPAssign.h"
#include "ShapeConverter.h"
#include "Utils.h"

#include "prt/API.h"

#include "CH/CH_Manager.h"

#include <limits>
#include <random>
#include <string>
#include <vector>

namespace {

constexpr const wchar_t* CGA_ANNOTATION_START_RULE = L"@StartRule";
constexpr const size_t CGA_NO_START_RULE_FOUND = size_t(-1);

using StringPairVector = std::vector<std::pair<std::string, std::string>>;
bool compareSecond(const StringPairVector::value_type& a, const StringPairVector::value_type& b) {
	return (a.second < b.second);
}

/**
 * find start rule (first annotated start rule or just first rule as fallback)
 */
std::wstring findStartRule(const RuleFileInfoUPtr& info) {
	const size_t numRules = info->getNumRules();
	assert(numRules > 0);

	auto startRuleIdx = CGA_NO_START_RULE_FOUND;
	for (size_t ri = 0; ri < numRules; ri++) {
		const prt::RuleFileInfo::Entry* re = info->getRule(ri);
		for (size_t ai = 0; ai < re->getNumAnnotations(); ai++) {
			if (std::wcscmp(re->getAnnotation(ai)->getName(), CGA_ANNOTATION_START_RULE) == 0) {
				startRuleIdx = ri;
				break;
			}
		}
	}

	if (startRuleIdx == CGA_NO_START_RULE_FOUND)
		startRuleIdx = 0; // use first rule as fallback

	const prt::RuleFileInfo::Entry* re = info->getRule(startRuleIdx);
	return {re->getName()};
}

constexpr const int NOT_CHANGED = 0;
constexpr const int CHANGED = 1;

std::string extractStyle(const prt::RuleFileInfo::Entry* re) {
	auto [style, name] = tokenizeFirst(re->getName(), NameConversion::STYLE_SEPARATOR);
	return toOSNarrowFromUTF16(style);
}

} // namespace

namespace CommonNodeParams {

prt::LogLevel getLogLevel(const OP_Node* node, fpreal t) {
	const auto ord = node->evalInt(LOG_LEVEL.getToken(), 0, t);
	switch (ord) {
		case 1:
			return prt::LOG_DEBUG;
		case 2:
			return prt::LOG_INFO;
		case 3:
			return prt::LOG_WARNING;
		case 4:
			return prt::LOG_ERROR;
		case 5:
			return prt::LOG_FATAL;
		case 6:
			return prt::LOG_NO;
		default:
			break;
	}
	return logging::getDefaultLogLevel();
};

} // namespace CommonNodeParams

namespace AssignNodeParams {
UT_String getPrimClsName(const OP_Node* node, fpreal t) {
	UT_String s;
	node->evalString(s, PRIM_CLS.getToken(), 0, t);
	return s;
}

void setPrimClsName(OP_Node* node, const UT_String& name, fpreal t) {
	node->setString(name, CH_STRING_LITERAL, PRIM_CLS.getToken(), 0, t);
}

/**
 * validates and updates all parameters/states depending on the rule package
 */
int updateRPK(void* data, int, fpreal32 time, const PRM_Template*) {
	auto* node = static_cast<SOPAssign*>(data);
	const PRTContextUPtr& prtCtx = node->getPRTCtx();

	UT_String utNextRPKStr;
	node->evalString(utNextRPKStr, AssignNodeParams::RPK.getToken(), 0, time);
	const std::filesystem::path nextRPK(utNextRPKStr.toStdString());

	ResolveMapSPtr resolveMap = prtCtx->getResolveMap(nextRPK);
	if (!resolveMap) {
		LOG_WRN << "invalid resolve map";
		return NOT_CHANGED;
	}

	// -- try get first rule file

	auto cgb = getCGB(resolveMap); // key -> uri
	if (!cgb) {
		LOG_ERR << "no rule files found in rule package";
		return NOT_CHANGED;
	}
	const std::wstring cgbKey = cgb->first;
	const std::wstring cgbURI = cgb->second;
	LOG_DBG << "cgbKey = " << cgbKey << ", "
	        << "cgbURI = " << cgbURI;

	// -- get start rule style/name
	prt::Status status = prt::STATUS_UNSPECIFIED_ERROR;
	const RuleFileInfoUPtr ruleFileInfo(
	        prt::createRuleFileInfo(cgbURI.c_str(), prtCtx->mPRTCache.get(), &status)); // TODO: cache
	if (!ruleFileInfo || (status != prt::STATUS_OK) || (ruleFileInfo->getNumRules() == 0)) {
		LOG_ERR << "failed to get rule file info or rule file does not contain any rules";
		return NOT_CHANGED;
	}

	const std::wstring fqStartRule = findStartRule(ruleFileInfo);
	const auto [startRuleStyle, startRuleName] = tokenizeFirst(fqStartRule, NameConversion::STYLE_SEPARATOR);
	LOG_DBG << "start rule: style = " << startRuleStyle << ", name = " << startRuleName;

	// -- update the node
	AssignNodeParams::setStyle(node, startRuleStyle, time);
	AssignNodeParams::setStartRule(node, startRuleName, time);

	// reset was successful, try to optimize the cache
	prtCtx->mPRTCache->flushAll();

	return CHANGED;
}

std::filesystem::path getRPK(const OP_Node* node, fpreal t) {
	UT_String s;
	node->evalString(s, RPK.getToken(), 0, t);
	return s.toStdString();
}

void buildStyleMenu(void* data, PRM_Name* theMenu, int theMaxSize, const PRM_SpareData*, const PRM_Parm*) {
	const auto* node = static_cast<SOPAssign*>(data);
	const PRTContextUPtr& prtCtx = node->getPRTCtx();

	const fpreal now = CHgetEvalTime();
	const std::filesystem::path rpk = getRPK(node, now);

	ResolveMapSPtr resolveMap = prtCtx->getResolveMap(rpk);
	if (!resolveMap) {
		theMenu[0].setTokenAndLabel(nullptr, nullptr);
		return;
	}

	auto cgb = getCGB(resolveMap); // key -> uri
	if (!cgb) {
		theMenu[0].setTokenAndLabel(nullptr, nullptr);
		return;
	}
	const std::wstring cgbURI = cgb->second;

	prt::Status status = prt::STATUS_UNSPECIFIED_ERROR;
	RuleFileInfoUPtr rfi(prt::createRuleFileInfo(cgbURI.c_str(), nullptr, &status));
	if (rfi && (status == prt::STATUS_OK)) {
		std::set<std::string> styles;
		for (size_t ri = 0; ri < rfi->getNumRules(); ri++) {
			const prt::RuleFileInfo::Entry* re = rfi->getRule(ri);
			styles.emplace(extractStyle(re));
		}
		for (size_t ai = 0; ai < rfi->getNumAttributes(); ai++) {
			const prt::RuleFileInfo::Entry* re = rfi->getAttribute(ai);
			styles.emplace(extractStyle(re));
		}
		size_t si = 0;
		for (const auto& s : styles) {
			theMenu[si].setTokenAndLabel(s.c_str(), s.c_str());
			si++;
		}
		const size_t limit = std::min<size_t>(styles.size(), static_cast<size_t>(theMaxSize));
		theMenu[limit].setTokenAndLabel(nullptr, nullptr); // need a null terminator
	}
}

std::wstring getStyle(const OP_Node* node, fpreal t) {
	UT_String s;
	node->evalString(s, STYLE.getToken(), 0, t);
	return toUTF16FromOSNarrow(s.toStdString());
}

void setStyle(OP_Node* node, const std::wstring& s, fpreal t) {
	const UT_String val(toOSNarrowFromUTF16(s));
	node->setString(val, CH_STRING_LITERAL, STYLE.getToken(), 0, t);
}

void buildStartRuleMenu(void* data, PRM_Name* theMenu, int theMaxSize, const PRM_SpareData*, const PRM_Parm* parm) {
	constexpr bool DBG = false;

	const auto* node = static_cast<SOPAssign*>(data);
	const PRTContextUPtr& prtCtx = node->getPRTCtx();

	const fpreal now = CHgetEvalTime();
	const std::filesystem::path rpk = getRPK(node, now);

	if (rpk.empty()) {
		theMenu[0].setTokenAndLabel(nullptr, nullptr);
		return;
	}

	ResolveMapSPtr resolveMap = prtCtx->getResolveMap(rpk);
	if (!resolveMap) {
		theMenu[0].setTokenAndLabel(nullptr, nullptr);
		return;
	}

	auto cgb = getCGB(resolveMap);
	if (!cgb) {
		theMenu[0].setTokenAndLabel(nullptr, nullptr);
		return;
	}
	const std::wstring ruleFile = cgb->first;
	const std::wstring cgbURI = cgb->second;

	if (DBG) {
		LOG_DBG << "buildStartRuleMenu";
		LOG_DBG << "   mRPK = " << rpk;
		LOG_DBG << "   mRuleFile = " << ruleFile;
	}

	prt::Status status = prt::STATUS_UNSPECIFIED_ERROR;
	RuleFileInfoUPtr rfi(prt::createRuleFileInfo(cgbURI.c_str(), nullptr, &status));
	if (status == prt::STATUS_OK) {
		StringPairVector startRules, rules;
		for (size_t ri = 0; ri < rfi->getNumRules(); ri++) {
			const prt::RuleFileInfo::Entry* re = rfi->getRule(ri);
			const std::wstring wRuleName = NameConversion::removeStyle(re->getName());
			const std::string rn = toOSNarrowFromUTF16(wRuleName);

			bool hasStartRuleAnnotation = false;
			for (size_t ai = 0; ai < re->getNumAnnotations(); ai++) {
				if (std::wcscmp(re->getAnnotation(ai)->getName(), CGA_ANNOTATION_START_RULE) == 0) {
					hasStartRuleAnnotation = true;
					break;
				}
			}

			if (hasStartRuleAnnotation)
				startRules.emplace_back(rn, rn + " (@StartRule)");
			else
				rules.emplace_back(rn, rn);
		}

		std::sort(startRules.begin(), startRules.end(), compareSecond);
		std::sort(rules.begin(), rules.end(), compareSecond);
		rules.reserve(rules.size() + startRules.size());
		rules.insert(rules.begin(), startRules.begin(), startRules.end());

		const size_t limit = std::min<size_t>(rules.size(), static_cast<size_t>(theMaxSize));
		for (size_t ri = 0; ri < limit; ri++) {
			theMenu[ri].setTokenAndLabel(rules[ri].first.c_str(), rules[ri].second.c_str());
		}
		theMenu[limit].setTokenAndLabel(nullptr, nullptr); // need a null terminator
	}
}

std::wstring getStartRule(const OP_Node* node, fpreal t) {
	UT_String s;
	node->evalString(s, START_RULE.getToken(), 0, t);
	return toUTF16FromOSNarrow(s.toStdString());
}

void setStartRule(OP_Node* node, const std::wstring& s, fpreal t) {
	const UT_String val(toOSNarrowFromUTF16(s));
	node->setString(val, CH_STRING_LITERAL, START_RULE.getToken(), 0, t);
}

int generateNewSeed(void* data, int, fpreal32 t, const PRM_Template*) {
	auto* node = static_cast<SOPAssign*>(data);
	std::random_device rd;
	std::uniform_int_distribution<int> dist(std::numeric_limits<int>::lowest(), std::numeric_limits<int>::max());
	const int32_t randomValue = dist(rd);
	node->setInt(SEED.getToken(), 0, t, randomValue);

	return CHANGED;
}

int getSeed(const OP_Node* node, fpreal t) {
	return node->evalInt(SEED.getToken(), 0, t);
}

bool getOverrideSeed(const OP_Node* node, fpreal t) {
	return static_cast<bool>(node->evalInt(OVERRIDE_SEED.getToken(), 0, t));
}

} // namespace AssignNodeParams

namespace GenerateNodeParams {

GroupCreation getGroupCreation(const OP_Node* node, fpreal t) {
	const auto ord = node->evalInt(GROUP_CREATION.getToken(), 0, t);
	switch (ord) {
		case 0:
			return GroupCreation::NONE;
		case 1:
			return GroupCreation::PRIMCLS;
		default:
			return GroupCreation::NONE;
	}
};

} // namespace GenerateNodeParams