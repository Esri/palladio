#pragma once

#include "client/sop.h"
#include "client/utils.h"

#include "PRM/PRM_ChoiceList.h"
#include "PRM/PRM_Parm.h"
#include "PRM/PRM_SpareData.h"

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
		PRM_Name(0)
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
		PRM_Name(0)
};
static PRM_ChoiceList logMenu((PRM_ChoiceListType)(PRM_CHOICELIST_EXCLUSIVE | PRM_CHOICELIST_REPLACE), logNames);
static PRM_Default logDefault(0, "ERROR");

// -- MULTI PARMS FOR DYNAMIC RULE ATTRIBUTE PARAMETER
static PRM_Name NODE_MULTIPARAM_FLOAT_NUM	("cgaFltNum",		"Number of Float Attributes");
static PRM_Name NODE_MULTIPARAM_FLOAT_ATTR	("cgaFltAttr#",		"Float Attr #");
static PRM_Name NODE_MULTIPARAM_FLOAT_VAL	("cgaFltVal#",		"Float Value #");
static PRM_Name NODE_MULTIPARAM_FLOAT_RESET	("cgaFltReset#",	"Reset to Rule");
static PRM_Name NODE_MULTIPARAM_STRING_NUM	("cgaStrNum",		"Number of String Attributes");
static PRM_Name NODE_MULTIPARAM_STRING_ATTR	("cgaStrAttr#",		"String Attr #");
static PRM_Name NODE_MULTIPARAM_STRING_VAL	("cgaStrVal#",		"String Value #");
static PRM_Name NODE_MULTIPARAM_STRING_RESET("cgaStrReset#",	"Reset to Rule");
static PRM_Name NODE_MULTIPARAM_BOOL_NUM	("cgaBoolNum",		"Number of Boolean Attributes");
static PRM_Name NODE_MULTIPARAM_BOOL_ATTR	("cgaBoolAttr#",	"Boolean Attr #");
static PRM_Name NODE_MULTIPARAM_BOOL_VAL	("cgaBoolVal#",		"Boolan Value #");
static PRM_Name NODE_MULTIPARAM_BOOL_RESET	("cgaBoolReset#",	"Reset to Rule");
int	resetRuleAttr(void *data, int index, fpreal64 time, const PRM_Template *tplate);
static PRM_Template NODE_MULTIPARAM_FLOAT_ATTR_TEMPLATE[] = {
		PRM_Template(PRM_STRING, 1, &NODE_MULTIPARAM_FLOAT_ATTR, PRMoneDefaults),
		PRM_Template(PRM_FLT, 1, &NODE_MULTIPARAM_FLOAT_VAL, PRMoneDefaults),
		PRM_Template(PRM_CALLBACK_NOREFRESH, 1, &NODE_MULTIPARAM_FLOAT_RESET, PRMoneDefaults, nullptr, nullptr, PRM_Callback(&resetRuleAttr)),
		PRM_Template()
};
static PRM_Template NODE_MULTIPARAM_STRING_ATTR_TEMPLATE[] = {
		PRM_Template(PRM_STRING, 1, &NODE_MULTIPARAM_STRING_ATTR, PRMoneDefaults),
		PRM_Template(PRM_STRING, 1, &NODE_MULTIPARAM_STRING_VAL, PRMoneDefaults),
		PRM_Template(PRM_CALLBACK_NOREFRESH, 1, &NODE_MULTIPARAM_STRING_RESET, PRMoneDefaults, nullptr, nullptr, PRM_Callback(&resetRuleAttr)),
		PRM_Template()
};
static PRM_Template NODE_MULTIPARAM_BOOL_ATTR_TEMPLATE[] = {
		PRM_Template(PRM_STRING, 1, &NODE_MULTIPARAM_BOOL_ATTR, PRMoneDefaults),
		PRM_Template(PRM_TOGGLE, 1, &NODE_MULTIPARAM_BOOL_VAL, PRMoneDefaults),
		PRM_Template(PRM_CALLBACK_NOREFRESH, 1, &NODE_MULTIPARAM_BOOL_RESET, PRMoneDefaults, nullptr, nullptr, PRM_Callback(&resetRuleAttr)),
		PRM_Template()
};

// -- MAIN NODE PARAMETER TEMPLATE
static PRM_Template NODE_PARAM_TEMPLATES[] = {
		// primitive classifier attribute
		PRM_Template(PRM_STRING, 1, &NODE_PARAM_SHAPE_CLS_ATTR, PRMoneDefaults),
		PRM_Template(PRM_ORD, PRM_Template::PRM_EXPORT_MAX, 1, &NODE_PARAM_SHAPE_CLS_TYPE, &shapeClsTypeDefault, &shapeClsTypeMenu),

		// rpk, rulefile, startrule, ...
		PRM_Template(PRM_FILE, 1, &NODE_PARAM_RPK, &rpkDefault, nullptr, nullptr, 0, &PRM_SpareData::fileChooserModeRead),
		PRM_Template(PRM_STRING, 1, &NODE_PARAM_RULE_FILE, PRMoneDefaults, &ruleFileMenu),
		PRM_Template(PRM_STRING, 1, &NODE_PARAM_STYLE, PRMoneDefaults),
		PRM_Template(PRM_STRING, 1, &NODE_PARAM_START_RULE, PRMoneDefaults, &startRuleMenu),
		PRM_Template(PRM_INT, 1, &NODE_PARAM_SEED, PRMoneDefaults),

		// logger
		PRM_Template(PRM_ORD, PRM_Template::PRM_EXPORT_MAX, 1, &NODE_PARAM_LOG, &logDefault, &logMenu),

		// rule attribute multiparm
		PRM_Template((PRM_MultiType)(PRM_MULTITYPE_LIST | PRM_MULTITYPE_NO_CONTROL_UI), NODE_MULTIPARAM_FLOAT_ATTR_TEMPLATE, 1, &NODE_MULTIPARAM_FLOAT_NUM, PRMoneDefaults, nullptr, &PRM_SpareData::multiStartOffsetZero),
		PRM_Template((PRM_MultiType)(PRM_MULTITYPE_LIST | PRM_MULTITYPE_NO_CONTROL_UI), NODE_MULTIPARAM_STRING_ATTR_TEMPLATE, 1, &NODE_MULTIPARAM_STRING_NUM, PRMoneDefaults, nullptr, &PRM_SpareData::multiStartOffsetZero),
		PRM_Template((PRM_MultiType)(PRM_MULTITYPE_LIST | PRM_MULTITYPE_NO_CONTROL_UI), NODE_MULTIPARAM_BOOL_ATTR_TEMPLATE, 1, &NODE_MULTIPARAM_BOOL_NUM, PRMoneDefaults, nullptr, &PRM_SpareData::multiStartOffsetZero),

		PRM_Template()
};

} // namespace p4h
