#include "SOPAssign.h"
#include "parameter.h"
#include "ShapeData.h"


namespace {

typedef std::vector<std::pair<std::string,std::string>> StringPairVector;
bool compareSecond (const StringPairVector::value_type& a, const StringPairVector::value_type& b) {
	return ( a.second < b.second );
}

} // namespace


namespace p4h {

void buildStartRuleMenu(void* data, PRM_Name* theMenu, int theMaxSize, const PRM_SpareData*, const PRM_Parm*) {
	static const bool DBG = false;

	const auto* node = static_cast<SOPAssign*>(data);
	const auto& ssd = node->getSharedShapeData();
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

	prt::Status status = prt::STATUS_UNSPECIFIED_ERROR;
	RuleFileInfoUPtr rfi(prt::createRuleFileInfo(cgbURI, nullptr, &status)); // TODO: caching
	if (status == prt::STATUS_OK) {
		StringPairVector startRules, rules;
		for (size_t ri = 0; ri < rfi->getNumRules(); ri++) {
			const prt::RuleFileInfo::Entry* re = rfi->getRule(ri);
			std::string rn = utils::toOSNarrowFromUTF16(re->getName());
			std::vector<std::string> tok;
			boost::split(tok, rn, boost::is_any_of("$"));

			bool hasStartRuleAnnotation = false;
			for (size_t ai = 0; ai < re->getNumAnnotations(); ai++) {
				if (std::wcscmp(re->getAnnotation(ai)->getName(), L"@StartRule") == 0) {
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

		const size_t limit = std::min<size_t>(rules.size(), theMaxSize);
		for (size_t ri = 0; ri < limit; ri++) {
			theMenu[ri].setToken(rules[ri].first.c_str());
			theMenu[ri].setLabel(rules[ri].second.c_str());
		}
		theMenu[limit].setToken(nullptr); // need a null terminator
	}
}

void buildRuleFileMenu(void* data, PRM_Name* theMenu, int theMaxSize, const PRM_SpareData*, const PRM_Parm*) {
	const auto* node = static_cast<SOPAssign*>(data);
	const auto& ssd = node->getSharedShapeData();
	const auto& prtCtx = node->getPRTCtx();

	if (ssd->mRPK.empty()) {
		theMenu[0].setToken(nullptr);
		return;
	}

	std::vector<std::pair<std::wstring,std::wstring>> cgbs; // key -> uri
	const ResolveMapUPtr& resolveMap = prtCtx->getResolveMap(ssd->mRPK);
	utils::getCGBs(resolveMap, cgbs);

	const size_t limit = std::min<size_t>(cgbs.size(), theMaxSize);
	for (size_t ri = 0; ri < limit; ri++) {
		std::string tok = utils::toOSNarrowFromUTF16(cgbs[ri].first);
		std::string lbl = utils::toOSNarrowFromUTF16(cgbs[ri].first); // TODO
		theMenu[ri].setToken(tok.c_str());
		theMenu[ri].setLabel(lbl.c_str());
	}
	theMenu[limit].setToken(nullptr); // need a null terminator
}

std::string extractStyle(const prt::RuleFileInfo::Entry* re) {
	std::string rn = utils::toOSNarrowFromUTF16(re->getName());
	std::vector<std::string> tok;
	boost::split(tok, rn, boost::is_any_of("$"));
	return tok[0];
}

void buildStyleMenu(void* data, PRM_Name* theMenu, int theMaxSize, const PRM_SpareData*, const PRM_Parm*) {
	const auto* node = static_cast<SOPAssign*>(data);
	const auto& ssd = node->getSharedShapeData();

	if (ssd->mRPK.empty() || ssd->mRuleFile.empty()) {
		theMenu[0].setToken(nullptr);
		return;
	}

	const PRTContextUPtr& prtCtx = node->getPRTCtx();
	const ResolveMapUPtr& resolveMap = prtCtx->getResolveMap(ssd->mRPK);
	const wchar_t* cgbURI = resolveMap->getString(ssd->mRuleFile.c_str());
	if (cgbURI == nullptr) {
		LOG_ERR << L"failed to resolve rule file '" << ssd->mRuleFile << "', aborting.";
		return;
	}

	prt::Status status = prt::STATUS_UNSPECIFIED_ERROR;
	RuleFileInfoUPtr rfi(prt::createRuleFileInfo(cgbURI, nullptr, &status)); // TODO: caching
	if (status == prt::STATUS_OK) {
		std::set<std::string> styles;
		for (size_t ri = 0; ri < rfi->getNumRules(); ri++) {
			const prt::RuleFileInfo::Entry* re = rfi->getRule(ri);
			styles.emplace(extractStyle(re));
		}
 		for (size_t ai = 0; ai < rfi->getNumAttributes(); ai++) {
			const prt::RuleFileInfo::Entry* re = rfi->getAttribute(ai);
			styles.emplace(extractStyle(re));
		}
		const size_t limit = std::min<size_t>(styles.size(), theMaxSize);
		size_t si = 0;
		for (const auto& s : styles) {
			theMenu[si].setToken(s.c_str());
			theMenu[si].setLabel(s.c_str());
			si++;
		}
		theMenu[limit].setToken(nullptr); // need a null terminator
	}
}

} // namespace p4h
