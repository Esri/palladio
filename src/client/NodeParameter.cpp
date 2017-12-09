#include "SOPAssign.h"
#include "NodeParameter.h"
#include "ShapeConverter.h"
#include "utils.h"

#include "prt/API.h"

#include "boost/algorithm/string.hpp"

#include <string>
#include <vector>


namespace {

constexpr const wchar_t* CGA_ANNOTATION_START_RULE = L"@StartRule";
constexpr const size_t   CGA_NO_START_RULE_FOUND   = size_t(-1);

using StringPairVector = std::vector<std::pair<std::string,std::string>>;
bool compareSecond (const StringPairVector::value_type& a, const StringPairVector::value_type& b) {
	return ( a.second < b.second );
}

/**
 * find start rule (first annotated start rule or just first rule as fallback)
 */
std::wstring findStartRule(const RuleFileInfoUPtr& info) {
	const size_t numRules = info->getNumRules();
	assert(numRules > 0);

	auto startRuleIdx = CGA_NO_START_RULE_FOUND;
	for (size_t ri = 0; ri < numRules; ri++) {
		const prt::RuleFileInfo::Entry *re = info->getRule(ri);
		for (size_t ai = 0; ai < re->getNumAnnotations(); ai++) {
			if (std::wcscmp(re->getAnnotation(ai)->getName(), CGA_ANNOTATION_START_RULE) == 0) {
				startRuleIdx = ri;
				break;
			}
		}
	}

	if (startRuleIdx == CGA_NO_START_RULE_FOUND)
		startRuleIdx = 0; // use first rule as fallback

	const prt::RuleFileInfo::Entry *re = info->getRule(startRuleIdx);
	return { re->getName() };
}

} // namespace


namespace AssignNodeParams {

/**
 * validates and updates all parameters/states depending on the rule package
 */
int resetRuleParameter(void *data, int, fpreal32 time, const PRM_Template*) {
	constexpr const int NOT_CHANGED = 0;
	constexpr const int CHANGED     = 1;

	auto* node = static_cast<SOPAssign*>(data);
	const PRTContextUPtr& prtCtx = node->getPRTCtx();

	UT_String utNextRPKStr;
	node->evalString(utNextRPKStr, AssignNodeParams::RPK.getToken(), 0, time);
	boost::filesystem::path nextRPK(utNextRPKStr.toStdString());

	// -- early exit if rpk path is not valid
	if (!boost::filesystem::exists(nextRPK))
		return NOT_CHANGED;

	const ResolveMapUPtr& resolveMap = prtCtx->getResolveMap(nextRPK);
	if (!resolveMap )
		return NOT_CHANGED;

	// -- try get first rule file
	std::vector<std::pair<std::wstring, std::wstring>> cgbs; // key -> uri
	getCGBs(resolveMap, cgbs);
	if (cgbs.empty()) {
		LOG_ERR << "no rule files found in rule package";
		return NOT_CHANGED;
	}
	const std::wstring cgbKey = cgbs.front().first;
	const std::wstring cgbURI = cgbs.front().second;
	LOG_DBG << "cgbKey = " << cgbKey << ", " << "cgbURI = " << cgbURI;

	prt::Status status = prt::STATUS_UNSPECIFIED_ERROR;
	RuleFileInfoUPtr ruleFileInfo(prt::createRuleFileInfo(cgbURI.c_str(), prtCtx->mPRTCache.get(), &status)); // TODO: cache
	if (!ruleFileInfo || (status != prt::STATUS_OK) || (ruleFileInfo->getNumRules() == 0)) {
		LOG_ERR << "failed to get rule file info or rule file does not contain any rules";
		return NOT_CHANGED;
	}
	const std::wstring fqStartRule = findStartRule(ruleFileInfo);

	// -- get style/name from start rule
	auto getStartRuleComponents = [](const std::wstring& fqRule) -> std::pair<std::wstring,std::wstring> {
		std::vector<std::wstring> startRuleComponents;
		boost::split(startRuleComponents, fqRule, boost::is_any_of(L"$")); // TODO: split is overkill
		return { startRuleComponents[0], startRuleComponents[1]};
	};
	const auto startRuleComponents = getStartRuleComponents(fqStartRule);
	LOG_DBG << "start rule: style = " << startRuleComponents.first << ", name = " << startRuleComponents.second;

	// -- update the node
	{
		UT_String val(toOSNarrowFromUTF16(cgbKey));
		node->setString(val, CH_STRING_LITERAL, AssignNodeParams::RULE_FILE.getToken(), 0, time);
	}
	{
		UT_String val(toOSNarrowFromUTF16(startRuleComponents.first));
		node->setString(val, CH_STRING_LITERAL, AssignNodeParams::STYLE.getToken(), 0, time);
	}
	{
		UT_String val(toOSNarrowFromUTF16(startRuleComponents.second));
		node->setString(val, CH_STRING_LITERAL, AssignNodeParams::START_RULE.getToken(), 0, time);
	}

	// reset was successful, try to optimize the cache
	prtCtx->mPRTCache->flushAll();

	return CHANGED;
}

void buildStartRuleMenu(void* data, PRM_Name* theMenu, int theMaxSize, const PRM_SpareData*, const PRM_Parm*) {
	constexpr const bool DBG = false;

	const auto* node = static_cast<SOPAssign*>(data);
	const auto& ssd = node->getShapeConverter();
	const PRTContextUPtr& prtCtx = node->getPRTCtx();

	if (DBG) {
		LOG_DBG << "buildStartRuleMenu";
		LOG_DBG << "   mRPK = " << ssd->mRPK;
		LOG_DBG << "   mRuleFile = " << ssd->mRuleFile;
	}

	if (ssd->mRPK.empty() || ssd->mRuleFile.empty()) {
		theMenu[0].setToken(nullptr);
		return;
	}

	const ResolveMapUPtr& resolveMap = prtCtx->getResolveMap(ssd->mRPK);
	const wchar_t* cgbURI = resolveMap->getString(ssd->mRuleFile.c_str());
	if (cgbURI == nullptr) {
		LOG_ERR << L"failed to resolve rule file '" << ssd->mRuleFile << "', aborting.";
		return;
	}

	// TODO: deduplicate with SOPAssign::resetRulePackage
	prt::Status status = prt::STATUS_UNSPECIFIED_ERROR;
	RuleFileInfoUPtr rfi(prt::createRuleFileInfo(cgbURI, nullptr, &status));
	if (status == prt::STATUS_OK) {
		StringPairVector startRules, rules;
		for (size_t ri = 0; ri < rfi->getNumRules(); ri++) {
			const prt::RuleFileInfo::Entry* re = rfi->getRule(ri);
			std::string rn = toOSNarrowFromUTF16(re->getName());
			std::vector<std::string> tok;
			boost::split(tok, rn, boost::is_any_of("$"));

			bool hasStartRuleAnnotation = false;
			for (size_t ai = 0; ai < re->getNumAnnotations(); ai++) {
				if (std::wcscmp(re->getAnnotation(ai)->getName(), CGA_ANNOTATION_START_RULE) == 0) {
					hasStartRuleAnnotation = true;
					break;
				}
			}

			if (hasStartRuleAnnotation)
				startRules.emplace_back(tok[1], tok[1] + " (@StartRule)");
			else
				rules.emplace_back(tok[1], tok[1]);
		}

		std::sort(startRules.begin(), startRules.end(), compareSecond);
		std::sort(rules.begin(), rules.end(), compareSecond);
		rules.reserve(rules.size() + startRules.size());
		rules.insert(rules.begin(), startRules.begin(), startRules.end());

		const size_t limit = std::min<size_t>(rules.size(), static_cast<size_t>(theMaxSize));
		for (size_t ri = 0; ri < limit; ri++) {
			theMenu[ri].setToken(rules[ri].first.c_str());
			theMenu[ri].setLabel(rules[ri].second.c_str());
		}
		theMenu[limit].setToken(nullptr); // need a null terminator
	}
}

void buildRuleFileMenu(void* data, PRM_Name* theMenu, int theMaxSize, const PRM_SpareData*, const PRM_Parm*) {
	const auto* node = static_cast<SOPAssign*>(data);
	const auto& ssd = node->getShapeConverter();
	const auto& prtCtx = node->getPRTCtx();

	const ResolveMapUPtr& resolveMap = prtCtx->getResolveMap(ssd->mRPK);
	if (!resolveMap)
		return;

	std::vector<std::pair<std::wstring,std::wstring>> cgbs; // key -> uri
	getCGBs(resolveMap, cgbs);

	const size_t limit = std::min<size_t>(cgbs.size(), static_cast<size_t>(theMaxSize));
	for (size_t ri = 0; ri < limit; ri++) {
		std::string tok = toOSNarrowFromUTF16(cgbs[ri].first);
		std::string lbl = toOSNarrowFromUTF16(cgbs[ri].first);
		theMenu[ri].setToken(tok.c_str());
		theMenu[ri].setLabel(lbl.c_str());
	}
	theMenu[limit].setToken(nullptr); // need a null terminator
}

std::string extractStyle(const prt::RuleFileInfo::Entry* re) {
	std::string rn = toOSNarrowFromUTF16(re->getName());
	std::vector<std::string> tok;
	boost::split(tok, rn, boost::is_any_of("$"));
	return tok[0];
}

void buildStyleMenu(void* data, PRM_Name* theMenu, int theMaxSize, const PRM_SpareData*, const PRM_Parm*) {
	const auto* node = static_cast<SOPAssign*>(data);
	const auto& ssd = node->getShapeConverter();
	const PRTContextUPtr& prtCtx = node->getPRTCtx();

	const ResolveMapUPtr& resolveMap = prtCtx->getResolveMap(ssd->mRPK);
	if (!resolveMap)
		return;

	const wchar_t* cgbURI = resolveMap->getString(ssd->mRuleFile.c_str());
	if (cgbURI == nullptr)
		return;

	prt::Status status = prt::STATUS_UNSPECIFIED_ERROR;
	RuleFileInfoUPtr rfi(prt::createRuleFileInfo(cgbURI, nullptr, &status));
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
			theMenu[si].setToken(s.c_str());
			theMenu[si].setLabel(s.c_str());
			si++;
		}
		const size_t limit = std::min<size_t>(styles.size(), static_cast<size_t>(theMaxSize));
		theMenu[limit].setToken(nullptr); // need a null terminator
	}
}

} // namespace p4h
