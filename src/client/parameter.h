#pragma once

#include "utils.h"

#include "PRM/PRM_ChoiceList.h"
#include "PRM/PRM_Parm.h"
#include "PRM/PRM_SpareData.h"
#include "PRM/PRM_Shared.h"

#include "boost/algorithm/string.hpp"


namespace p4h {

// -- RULE PACKAGE
static PRM_Name NODE_PARAM_RPK("rpk", "Rule Package");
static PRM_Default rpkDefault(0, "$HIP/$F.rpk");

// -- RULE FILE (cgb)
static PRM_Name NODE_PARAM_RULE_FILE("ruleFile", "Rule File");
void buildRuleFileMenu(void* data, PRM_Name* theMenu, int theMaxSize, const PRM_SpareData*, const PRM_Parm*);
static PRM_ChoiceList ruleFileMenu(static_cast<PRM_ChoiceListType>(PRM_CHOICELIST_EXCLUSIVE | PRM_CHOICELIST_REPLACE), &buildRuleFileMenu);

// -- STYLE
static PRM_Name NODE_PARAM_STYLE("style", "Style");
void buildStyleMenu(void* data, PRM_Name* theMenu, int theMaxSize, const PRM_SpareData*, const PRM_Parm*);
static PRM_ChoiceList styleMenu(static_cast<PRM_ChoiceListType>(PRM_CHOICELIST_EXCLUSIVE | PRM_CHOICELIST_REPLACE), &buildStyleMenu);

// -- START RULE
static PRM_Name NODE_PARAM_START_RULE("startRule", "Start Rule");
void buildStartRuleMenu(void* data, PRM_Name* theMenu, int theMaxSize, const PRM_SpareData*, const PRM_Parm*);
static PRM_ChoiceList startRuleMenu(static_cast<PRM_ChoiceListType>(PRM_CHOICELIST_EXCLUSIVE | PRM_CHOICELIST_REPLACE), &buildStartRuleMenu);

// -- RANDOM SEED
static PRM_Name NODE_PARAM_SEED("seed", "Random Seed");

// -- PRIMITIVE CLASSIFIERS
static PRM_Name NODE_PARAM_SHAPE_CLS_ATTR("shapeClsAttr", "Shape Classifier");
static PRM_Name NODE_PARAM_SHAPE_CLS_TYPE("shapeClsType", "Shape Classifier Type");
static PRM_Name shapeClsTypes[] = {
		PRM_Name("STRING", "String"),
		PRM_Name("INT", "Integer"),
		PRM_Name("FLOAT", "Float"),
		PRM_Name(nullptr)
};
static PRM_ChoiceList shapeClsTypeMenu((PRM_ChoiceListType)(PRM_CHOICELIST_EXCLUSIVE | PRM_CHOICELIST_REPLACE), shapeClsTypes);
static PRM_Default shapeClsTypeDefault(0, "INT");

// -- LOGGER
static PRM_Name NODE_PARAM_LOG("logLevel", "Log Level");
static PRM_Name logNames[] = {
		PRM_Name("TRACE", "trace"), // TODO: eventually, remove this and offset index by 1
		PRM_Name("DEBUG", "debug"),
		PRM_Name("INFO", "info"),
		PRM_Name("WARNING", "warning"),
		PRM_Name("ERROR", "error"),
		PRM_Name("FATAL", "fatal"),
		PRM_Name(nullptr)
};
static PRM_ChoiceList logMenu((PRM_ChoiceListType)(PRM_CHOICELIST_EXCLUSIVE | PRM_CHOICELIST_REPLACE), logNames);
static PRM_Default logDefault(0, "ERROR");

// -- ASSIGN NODE PARAMETER TEMPLATE
static PRM_Default DEFAULT_SHAPE_CLS_ATTR(0.0f, "shapeCls", CH_STRING_LITERAL);
static PRM_Template ASSIGN_NODE_PARAM_TEMPLATES[] = {
		// primitive classifier attribute
		PRM_Template(PRM_STRING, 1, &NODE_PARAM_SHAPE_CLS_ATTR, &DEFAULT_SHAPE_CLS_ATTR),
		PRM_Template(PRM_ORD, PRM_Template::PRM_EXPORT_MAX, 1, &NODE_PARAM_SHAPE_CLS_TYPE, &shapeClsTypeDefault, &shapeClsTypeMenu),

		// rpk, rulefile, startrule, ...
		PRM_Template(PRM_FILE, 1, &NODE_PARAM_RPK, &rpkDefault, nullptr, nullptr, 0, &PRM_SpareData::fileChooserModeRead),
		PRM_Template(PRM_STRING, 1, &NODE_PARAM_RULE_FILE, PRMoneDefaults, &ruleFileMenu),
		PRM_Template(PRM_STRING, 1, &NODE_PARAM_STYLE, PRMoneDefaults, &styleMenu),
		PRM_Template(PRM_STRING, 1, &NODE_PARAM_START_RULE, PRMoneDefaults, &startRuleMenu),
		PRM_Template(PRM_INT, 1, &NODE_PARAM_SEED, PRMoneDefaults),

		// logger
		PRM_Template(PRM_ORD, PRM_Template::PRM_EXPORT_MAX, 1, &NODE_PARAM_LOG, &logDefault, &logMenu),

		PRM_Template()
};

// -- GENERATE NODE PARAMETER TEMPLATE
static PRM_Name    GENERATE_NODE_PARAM_EMIT_ATTRS("emitAttrs", "Emit CGA attributes");
static PRM_Name    GENERATE_NODE_PARAM_EMIT_MATERIAL("emitMaterials", "Emit material attributes");
static PRM_Name    GENERATE_NODE_PARAM_EMIT_REPORTS("emitReports", "Emit CGA reports");
static PRM_Name    GENERATE_NODE_PARAM_EMIT_REPORT_SUMMARIES("emitReportSummaries", "Emit CGA report summaries");
static PRM_Template GENERATE_NODE_PARAM_TEMPLATES[] {
		PRM_Template(PRM_TOGGLE, 1, &GENERATE_NODE_PARAM_EMIT_ATTRS),
		PRM_Template(PRM_TOGGLE, 1, &GENERATE_NODE_PARAM_EMIT_MATERIAL),
		PRM_Template(PRM_TOGGLE, 1, &GENERATE_NODE_PARAM_EMIT_REPORTS),
		PRM_Template(PRM_TOGGLE, 1, &GENERATE_NODE_PARAM_EMIT_REPORT_SUMMARIES),
		PRM_Template()
};

} // namespace p4h
