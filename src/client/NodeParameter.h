#pragma once

#include "PRM/PRM_ChoiceList.h"
#include "PRM/PRM_Parm.h"
#include "PRM/PRM_SpareData.h"
#include "PRM/PRM_Shared.h"


namespace AssignNodeParams {

// -- RULE PACKAGE
static PRM_Name RPK("rpk", "Rule Package");
static PRM_Default rpkDefault(0, "$HIP/$F.rpk");

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

// -- PRIMITIVE CLASSIFIERS
static PRM_Name SHAPE_CLS_ATTR("shapeClsAttr", "Shape Classifier");
static PRM_Name SHAPE_CLS_TYPE("shapeClsType", "Shape Classifier Type");
static PRM_Name shapeClsTypes[] = {
	PRM_Name("STRING", "String"),
	PRM_Name("INT", "Integer"),
	PRM_Name("FLOAT", "Float"),
	PRM_Name(nullptr)
};
static PRM_ChoiceList shapeClsTypeMenu((PRM_ChoiceListType) (PRM_CHOICELIST_EXCLUSIVE | PRM_CHOICELIST_REPLACE),
                                       shapeClsTypes);
static PRM_Default shapeClsTypeDefault(0, "INT");

// -- LOGGER
static PRM_Name LOG("logLevel", "Log Level");
static PRM_Name logNames[] = {
	PRM_Name("TRACE", "trace"), // TODO: eventually, remove this and offset index by 1
	PRM_Name("DEBUG", "debug"),
	PRM_Name("INFO", "info"),
	PRM_Name("WARNING", "warning"),
	PRM_Name("ERROR", "error"),
	PRM_Name("FATAL", "fatal"),
	PRM_Name(nullptr)
};
static PRM_ChoiceList logMenu((PRM_ChoiceListType) (PRM_CHOICELIST_EXCLUSIVE | PRM_CHOICELIST_REPLACE), logNames);
static PRM_Default logDefault(0, "ERROR");

static PRM_Default DEFAULT_SHAPE_CLS_ATTR(0.0f, "shapeCls", CH_STRING_LITERAL);
static PRM_Template PARAM_TEMPLATES[] = {
		// primitive classifier attribute
		PRM_Template(PRM_STRING, 1, &SHAPE_CLS_ATTR, &DEFAULT_SHAPE_CLS_ATTR),
		PRM_Template(PRM_ORD, PRM_Template::PRM_EXPORT_MAX, 1, &SHAPE_CLS_TYPE, &shapeClsTypeDefault, &shapeClsTypeMenu),

		// rpk, rulefile, startrule, ...
		PRM_Template(PRM_FILE,   1, &RPK,        &rpkDefault, nullptr, nullptr, 0, &PRM_SpareData::fileChooserModeRead),
		PRM_Template(PRM_STRING, 1, &RULE_FILE,  PRMoneDefaults, &ruleFileMenu),
		PRM_Template(PRM_STRING, 1, &STYLE,      PRMoneDefaults, &styleMenu),
		PRM_Template(PRM_STRING, 1, &START_RULE, PRMoneDefaults, &startRuleMenu),
		PRM_Template(PRM_INT,    1, &SEED,       PRMoneDefaults),

		// logger
		PRM_Template(PRM_ORD, PRM_Template::PRM_EXPORT_MAX, 1, &LOG, &logDefault, &logMenu),

		PRM_Template()
};

} // namespace AssignNodeParams


namespace GenerateNodeParams {

static PRM_Name EMIT_ATTRS("emitAttrs", "Emit CGA attributes");
static PRM_Name EMIT_MATERIAL("emitMaterials", "Emit material attributes");
static PRM_Name EMIT_REPORTS("emitReports", "Emit CGA reports");
static PRM_Name EMIT_REPORT_SUMMARIES("emitReportSummaries", "Emit CGA report summaries");
static PRM_Template PARAM_TEMPLATES[] {
		PRM_Template(PRM_TOGGLE, 1, &EMIT_ATTRS),
		PRM_Template(PRM_TOGGLE, 1, &EMIT_MATERIAL),
		PRM_Template(PRM_TOGGLE, 1, &EMIT_REPORTS),
		PRM_Template(PRM_TOGGLE, 1, &EMIT_REPORT_SUMMARIES),
		PRM_Template()
};

} // namespace GenerateNodeParams
