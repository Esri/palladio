#include "client/parameter.h"


namespace {

typedef std::vector<std::pair<std::string,std::string>> StringPairVector;
bool compareSecond (const StringPairVector::value_type& a, const StringPairVector::value_type& b) {
	return ( a.second < b.second );
}

} // namespace

namespace p4h {

void buildStartRuleMenu(void* data, PRM_Name* theMenu, int theMaxSize, const PRM_SpareData*, const PRM_Parm*) {
	static const bool DBG = false;

	SOP_PRT* node = static_cast<SOP_PRT*>(data);
	const InitialShapeContext& isCtx = node->getInitialShapeCtx();

	if (DBG) {
		LOG_DBG << "buildStartRuleMenu";
		LOG_DBG << "   mRPK = " << isCtx.mRPK;
		LOG_DBG << "   mRuleFile = " << isCtx.mRuleFile;
	}

	if (isCtx.mAssetsMap == nullptr || isCtx.mRPK.empty() || isCtx.mRuleFile.empty()) {
		theMenu[0].setToken(0);
		return;
	}

	const wchar_t* cgbURI = isCtx.mAssetsMap->getString(isCtx.mRuleFile.c_str());
	if (cgbURI == nullptr) {
		LOG_ERR << L"failed to resolve rule file '" << isCtx.mRuleFile << "', aborting.";
		return;
	}

	prt::Status status = prt::STATUS_UNSPECIFIED_ERROR;
	const prt::RuleFileInfo* rfi = prt::createRuleFileInfo(cgbURI, nullptr, &status);
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
		theMenu[limit].setToken(0); // need a null terminator
		rfi->destroy();
	}
}

void buildRuleFileMenu(void* data, PRM_Name* theMenu, int theMaxSize, const PRM_SpareData*, const PRM_Parm*) {
	SOP_PRT* node = static_cast<SOP_PRT*>(data);
	const InitialShapeContext& isCtx = node->getInitialShapeCtx();

	if (!isCtx.mAssetsMap || isCtx.mRPK.empty()) {
		theMenu[0].setToken(0);
		return;
	}

	std::vector<std::pair<std::wstring,std::wstring>> cgbs; // key -> uri
	utils::getCGBs(isCtx.mAssetsMap, cgbs);

	const size_t limit = std::min<size_t>(cgbs.size(), theMaxSize);
	for (size_t ri = 0; ri < limit; ri++) {
		std::string tok = utils::toOSNarrowFromUTF16(cgbs[ri].first);
		std::string lbl = utils::toOSNarrowFromUTF16(cgbs[ri].first); // TODO
		theMenu[ri].setToken(tok.c_str());
		theMenu[ri].setLabel(lbl.c_str());
	}
	theMenu[limit].setToken(0); // need a null terminator
}

int	resetRuleAttr(void *data, int index, fpreal64 time, const PRM_Template *tplate) {
	p4h::SOP_PRT* node = static_cast<p4h::SOP_PRT*>(data);

	UT_String tok;
	tplate->getToken(tok);
	const char* suf = tok.suffix();
	LOG_DBG << "resetRuleAttr: tok = " << tok.toStdString() << ", suf = " << suf;
	UT_String ruleAttr;
	if (suf) {
		int idx = std::atoi(suf);
		const char* valueTok = nullptr;
		if (tok.startsWith("cgaFlt"))
			valueTok = NODE_MULTIPARAM_FLOAT_ATTR.getToken();
		else if (tok.startsWith("cgaStr"))
			valueTok = NODE_MULTIPARAM_STRING_ATTR.getToken();
		else if (tok.startsWith("cgaBool"))
			valueTok = NODE_MULTIPARAM_BOOL_ATTR.getToken();
		if (valueTok)
			node->evalStringInst(valueTok, &idx, ruleAttr, 0, 0.0);
		LOG_DBG << "    idx = " << idx << ", valueTok = " << valueTok << ", ruleAttr = " << ruleAttr;
	}
	if (ruleAttr.length() > 0)
		node->resetUserAttribute(ruleAttr.toStdString());
	return 1;
}

} // namespace p4h
