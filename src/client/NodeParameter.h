#pragma once

#include "PRM/PRM_ChoiceList.h"
#include "PRM/PRM_Parm.h"
#include "PRM/PRM_SpareData.h"
#include "PRM/PRM_Shared.h"
#include "PRM/PRM_Callback.h"
#include "GA/GA_Types.h"


namespace AssignNodeParams {

// -- PRIMITIVE CLASSIFIERS
static PRM_Name PARAM_NAME_PRIM_CLS_ATTR("primClsAttr", "Primitive Classifier");
static PRM_Name PARAM_NAME_PRIM_CLS_TYPE("primClsType", "Primitive Classifier Type");

static const char*           PRIM_CLS_TYPE_TOKENS[] = { "STRING", "INT", "FLOAT" };
static const char*           PRIM_CLS_TYPE_LABELS[] = { "String", "Integer", "Float" };
static const GA_StorageClass PRIM_CLS_TYPE_VALUES[]  = { GA_STORECLASS_STRING, GA_STORECLASS_INT, GA_STORECLASS_FLOAT };

static PRM_Name PRIM_CLS_TYPE_MENU_ITEMS[] = {
	PRM_Name(PRIM_CLS_TYPE_TOKENS[0], PRIM_CLS_TYPE_LABELS[0]),
	PRM_Name(PRIM_CLS_TYPE_TOKENS[1], PRIM_CLS_TYPE_LABELS[1]),
	PRM_Name(PRIM_CLS_TYPE_TOKENS[2], PRIM_CLS_TYPE_LABELS[2]),
	PRM_Name(nullptr)
};
static PRM_ChoiceList primClsTypeMenu((PRM_ChoiceListType) (PRM_CHOICELIST_EXCLUSIVE | PRM_CHOICELIST_REPLACE), PRIM_CLS_TYPE_MENU_ITEMS);

const size_t DEFAULT_PRIM_CLS_TYPE_ORDINAL = 1;
const GA_StorageClass DEFAULT_PRIM_CLS_TYPE_VALUE = PRIM_CLS_TYPE_VALUES[DEFAULT_PRIM_CLS_TYPE_ORDINAL];
static PRM_Default DEFAULT_PRIM_CLS_TYPE(0, PRIM_CLS_TYPE_TOKENS[DEFAULT_PRIM_CLS_TYPE_ORDINAL]);

static PRM_Default DEFAULT_PRIM_CLS_ATTR(0.0f, "primCls", CH_STRING_LITERAL);


// -- RULE PACKAGE
static PRM_Name RPK("rpk", "Rule Package");
static PRM_Default rpkDefault(0, "$HIP/$F.rpk");
int updateRPK(void* data, int index, fpreal32 time, const PRM_Template*);
static PRM_Callback rpkCallback(&updateRPK);


// -- RULE FILE (cgb)
static PRM_Name RULE_FILE("ruleFile", "Rule File");

void buildRuleFileMenu(void *data, PRM_Name *theMenu, int theMaxSize, const PRM_SpareData *, const PRM_Parm *);

static PRM_ChoiceList ruleFileMenu(static_cast<PRM_ChoiceListType>(PRM_CHOICELIST_EXCLUSIVE | PRM_CHOICELIST_REPLACE),
                                   &buildRuleFileMenu);

// -- STYLE
static PRM_Name STYLE("style", "Style");

void buildStyleMenu(void *data, PRM_Name *theMenu, int theMaxSize, const PRM_SpareData *, const PRM_Parm *);

static PRM_ChoiceList styleMenu(static_cast<PRM_ChoiceListType>(PRM_CHOICELIST_EXCLUSIVE | PRM_CHOICELIST_REPLACE),
                                &buildStyleMenu);

// -- START RULE
static PRM_Name START_RULE("startRule", "Start Rule");

void buildStartRuleMenu(void *data, PRM_Name *theMenu, int theMaxSize, const PRM_SpareData *, const PRM_Parm *);

static PRM_ChoiceList startRuleMenu(static_cast<PRM_ChoiceListType>(PRM_CHOICELIST_EXCLUSIVE | PRM_CHOICELIST_REPLACE),
                                    &buildStartRuleMenu);

// -- RANDOM SEED
static PRM_Name SEED("seed", "Random Seed");


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

static PRM_Name EMIT_ATTRS("emitAttrs", "Emit CGA attributes");
static PRM_Name EMIT_MATERIAL("emitMaterials", "Emit material attributes");
static PRM_Name EMIT_REPORTS("emitReports", "Emit CGA reports");
static PRM_Template PARAM_TEMPLATES[] {
		PRM_Template(PRM_TOGGLE, 1, &EMIT_ATTRS),
		PRM_Template(PRM_TOGGLE, 1, &EMIT_MATERIAL),
		PRM_Template(PRM_TOGGLE, 1, &EMIT_REPORTS),
		PRM_Template()
};

} // namespace GenerateNodeParams
