#pragma once

#include "utils.h"

#include "PRM/PRM_ChoiceList.h"
#include "PRM/PRM_Parm.h"
#include "PRM/PRM_SpareData.h"
#include "PRM/PRM_Shared.h"
#include "PRM/PRM_Callback.h"
#include "GA/GA_Types.h"
#include "OP/OP_Node.h"

#include "boost/filesystem/path.hpp"


namespace AssignNodeParams {

// -- PRIMITIVE CLASSIFIER NAME
static PRM_Name PARAM_NAME_PRIM_CLS_ATTR("primClsAttr", "Primitive Classifier");
static PRM_Default DEFAULT_PRIM_CLS_ATTR(0.0f, "primCls", CH_STRING_LITERAL);

auto getPrimClsName = [](const OP_Node* node, fpreal t) -> UT_String {
	UT_String s;
	node->evalString(s, PARAM_NAME_PRIM_CLS_ATTR.getToken(), 0, t);
	return s;
};

auto setPrimClsName = [](OP_Node* node, const UT_String& name, fpreal t) {
	node->setString(name, CH_STRING_LITERAL, PARAM_NAME_PRIM_CLS_ATTR.getToken(), 0, t);
};


// -- PRIMITIVE CLASSIFIER TYPE
static PRM_Name PARAM_NAME_PRIM_CLS_TYPE("primClsType", "Primitive Classifier Type");

static const char*           PRIM_CLS_TYPE_TOKENS[] = { "STRING", "INT" };
static const char*           PRIM_CLS_TYPE_LABELS[] = { "String", "Integer" };
static const GA_StorageClass PRIM_CLS_TYPE_VALUES[]  = { GA_STORECLASS_STRING, GA_STORECLASS_INT };

static PRM_Name PRIM_CLS_TYPE_MENU_ITEMS[] = {
	PRM_Name(PRIM_CLS_TYPE_TOKENS[0], PRIM_CLS_TYPE_LABELS[0]),
	PRM_Name(PRIM_CLS_TYPE_TOKENS[1], PRIM_CLS_TYPE_LABELS[1]),
	PRM_Name(nullptr)
};
static PRM_ChoiceList primClsTypeMenu((PRM_ChoiceListType) (PRM_CHOICELIST_EXCLUSIVE | PRM_CHOICELIST_REPLACE), PRIM_CLS_TYPE_MENU_ITEMS);

const size_t DEFAULT_PRIM_CLS_TYPE_ORDINAL = 1; // default prim cls is an int
const GA_StorageClass DEFAULT_PRIM_CLS_TYPE_VALUE = PRIM_CLS_TYPE_VALUES[DEFAULT_PRIM_CLS_TYPE_ORDINAL];
static PRM_Default DEFAULT_PRIM_CLS_TYPE(0, PRIM_CLS_TYPE_TOKENS[DEFAULT_PRIM_CLS_TYPE_ORDINAL]);

auto getPrimClsType = [](const OP_Node* node, fpreal t) -> GA_StorageClass {
	const auto typeChoice = node->evalInt(PARAM_NAME_PRIM_CLS_TYPE.getToken(), 0, t);
	switch (typeChoice) {
		case 0:  return GA_STORECLASS_STRING;  break;
		case 1:  return GA_STORECLASS_INT;     break;
		default: return GA_STORECLASS_INVALID; break;
	}
};


// -- RULE PACKAGE
static PRM_Name RPK("rpk", "Rule Package");
static PRM_Default rpkDefault(0, "");
int updateRPK(void* data, int index, fpreal32 time, const PRM_Template*);
static PRM_Callback rpkCallback(&updateRPK);

auto getRPK = [](const OP_Node* node, fpreal t) -> boost::filesystem::path {
	UT_String s;
	node->evalString(s, RPK.getToken(), 0, t);
	return s.toStdString();
};


// -- RULE FILE (cgb)
static PRM_Name RULE_FILE("ruleFile", "Rule File");

void buildRuleFileMenu(void *data, PRM_Name *theMenu, int theMaxSize, const PRM_SpareData *, const PRM_Parm *);

static PRM_ChoiceList ruleFileMenu(static_cast<PRM_ChoiceListType>(PRM_CHOICELIST_EXCLUSIVE | PRM_CHOICELIST_REPLACE),
                                   &buildRuleFileMenu);

auto getRuleFile = [](const OP_Node* node, fpreal t) -> std::wstring {
	UT_String s;
	node->evalString(s, RULE_FILE.getToken(), 0, t);
	return toUTF16FromOSNarrow(s.toStdString());
};

auto setRuleFile = [](OP_Node* node, const std::wstring& ruleFile, fpreal t) {
	const UT_String val(toOSNarrowFromUTF16(ruleFile));
	node->setString(val, CH_STRING_LITERAL, RULE_FILE.getToken(), 0, t);
};


// -- STYLE
static PRM_Name STYLE("style", "Style");

void buildStyleMenu(void *data, PRM_Name *theMenu, int theMaxSize, const PRM_SpareData *, const PRM_Parm *);

static PRM_ChoiceList styleMenu(static_cast<PRM_ChoiceListType>(PRM_CHOICELIST_EXCLUSIVE | PRM_CHOICELIST_REPLACE),
                                &buildStyleMenu);

auto getStyle = [](const OP_Node* node, fpreal t) -> std::wstring {
	UT_String s;
	node->evalString(s, STYLE.getToken(), 0, t);
	return toUTF16FromOSNarrow(s.toStdString());
};

auto setStyle = [](OP_Node* node, const std::wstring& s, fpreal t) {
	const UT_String val(toOSNarrowFromUTF16(s));
	node->setString(val, CH_STRING_LITERAL, STYLE.getToken(), 0, t);
};


// -- START RULE
static PRM_Name START_RULE("startRule", "Start Rule");

void buildStartRuleMenu(void *data, PRM_Name *theMenu, int theMaxSize, const PRM_SpareData *, const PRM_Parm *);

static PRM_ChoiceList startRuleMenu(static_cast<PRM_ChoiceListType>(PRM_CHOICELIST_EXCLUSIVE | PRM_CHOICELIST_REPLACE),
                                    &buildStartRuleMenu);

auto getStartRule = [](const OP_Node* node, fpreal t) -> std::wstring {
	UT_String s;
	node->evalString(s, START_RULE.getToken(), 0, t);
	return toUTF16FromOSNarrow(s.toStdString());
};

auto setStartRule = [](OP_Node* node, const std::wstring& s, fpreal t) {
	const UT_String val(toOSNarrowFromUTF16(s));
	node->setString(val, CH_STRING_LITERAL, START_RULE.getToken(), 0, t);
};


// -- RANDOM SEED
static PRM_Name SEED("seed", "Random Seed");

auto getSeed = [](const OP_Node* node, fpreal t) -> int64_t {
	return node->evalInt(SEED.getToken(), 0, t);
};


// -- ASSIGN NODE PARAMS
static PRM_Template PARAM_TEMPLATES[] = {
		// primitive classifier attribute
		PRM_Template(PRM_STRING,                            1, &PARAM_NAME_PRIM_CLS_ATTR, &DEFAULT_PRIM_CLS_ATTR),
		PRM_Template(PRM_ORD, PRM_Template::PRM_EXPORT_MAX, 1, &PARAM_NAME_PRIM_CLS_TYPE, &DEFAULT_PRIM_CLS_TYPE, &primClsTypeMenu),

		// rpk, rulefile, startrule, ...
		PRM_Template(PRM_FILE,   1, &RPK,        &rpkDefault,    nullptr, nullptr, rpkCallback, &PRM_SpareData::fileChooserModeRead),
		PRM_Template(PRM_STRING, 1, &RULE_FILE,  PRMoneDefaults, &ruleFileMenu),
		PRM_Template(PRM_STRING, 1, &STYLE,      PRMoneDefaults, &styleMenu),
		PRM_Template(PRM_STRING, 1, &START_RULE, PRMoneDefaults, &startRuleMenu),
		PRM_Template(PRM_INT,    1, &SEED,       PRMoneDefaults),

		PRM_Template()
};

} // namespace AssignNodeParams


namespace GenerateNodeParams {

static PRM_Name GROUP_CREATION("groupCreation", "Primitive Groups");
static const char* GROUP_CREATION_TOKENS[] = { "NONE", "PRIMCLS" };
static const char* GROUP_CREATION_LABELS[] = {
	"Do not create groups",
	"One group per primitive classifier"
};
static PRM_Name GROUP_CREATION_MENU_ITEMS[] = {
	PRM_Name(GROUP_CREATION_TOKENS[0], GROUP_CREATION_LABELS[0]),
	PRM_Name(GROUP_CREATION_TOKENS[1], GROUP_CREATION_LABELS[1]),
	PRM_Name(nullptr)
};
static PRM_ChoiceList groupCreationMenu((PRM_ChoiceListType)(PRM_CHOICELIST_EXCLUSIVE | PRM_CHOICELIST_REPLACE), GROUP_CREATION_MENU_ITEMS);
const size_t DEFAULT_GROUP_CREATION_ORDINAL = 0;
static PRM_Default DEFAULT_GROUP_CREATION(0, GROUP_CREATION_TOKENS[DEFAULT_GROUP_CREATION_ORDINAL]);

enum class GroupCreation { NONE, PRIMCLS };
auto getGroupCreation = [](const OP_Node* node, fpreal t) -> GroupCreation {
	const auto ord = node->evalInt(GROUP_CREATION.getToken(), 0, t);
	switch (ord) {
		case 0: return GroupCreation::NONE; break;
		case 1: return GroupCreation::PRIMCLS; break;
		default: return GroupCreation::NONE; break;
	}
};

static PRM_Name EMIT_ATTRS("emitAttrs", "Emit CGA attributes");
static PRM_Name EMIT_MATERIAL("emitMaterials", "Emit material attributes");
static PRM_Name EMIT_REPORTS("emitReports", "Emit CGA reports");
static PRM_Template PARAM_TEMPLATES[] {
		PRM_Template(PRM_ORD, PRM_Template::PRM_EXPORT_MAX, 1, &GROUP_CREATION, &DEFAULT_GROUP_CREATION, &groupCreationMenu),
		PRM_Template(PRM_TOGGLE, 1, &EMIT_ATTRS),
		PRM_Template(PRM_TOGGLE, 1, &EMIT_MATERIAL),
		PRM_Template(PRM_TOGGLE, 1, &EMIT_REPORTS),
		PRM_Template()
};

} // namespace GenerateNodeParams
