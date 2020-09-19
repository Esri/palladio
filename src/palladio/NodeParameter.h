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

#pragma once

#include "PrimitiveClassifier.h"
#include "ShapeConverter.h"
#include "Utils.h"

#include "GA/GA_Types.h"
#include "OP/OP_Node.h"
#include "PRM/PRM_Callback.h"
#include "PRM/PRM_ChoiceList.h"
#include "PRM/PRM_Parm.h"
#include "PRM/PRM_Shared.h"
#include "PRM/PRM_SpareData.h"

#include "prt/LogLevel.h"

// clang-format off
#include "BoostRedirect.h"
#include PLD_BOOST_INCLUDE(/filesystem/path.hpp)
#include PLD_BOOST_INCLUDE(/variant.hpp)
// clang-format on

namespace CommonNodeParams {

static PRM_Name LOG_LEVEL("logLevel", "Log Level");
static const char* LOG_LEVEL_TOKENS[] = {"Default", "Debug", "Info", "Warning", "Error", "Fatal", "None"};
static PRM_Name LOG_LEVEL_MENU_ITEMS[] = {PRM_Name(LOG_LEVEL_TOKENS[0]), PRM_Name(LOG_LEVEL_TOKENS[1]),
                                          PRM_Name(LOG_LEVEL_TOKENS[2]), PRM_Name(LOG_LEVEL_TOKENS[3]),
                                          PRM_Name(LOG_LEVEL_TOKENS[4]), PRM_Name(LOG_LEVEL_TOKENS[5]),
                                          PRM_Name(LOG_LEVEL_TOKENS[6]), PRM_Name(nullptr)};
static PRM_ChoiceList logLevelMenu((PRM_ChoiceListType)(PRM_CHOICELIST_EXCLUSIVE | PRM_CHOICELIST_REPLACE),
                                   LOG_LEVEL_MENU_ITEMS);
const size_t DEFAULT_LOG_LEVEL_ORDINAL = 0;
static PRM_Default DEFAULT_LOG_LEVEL(0, LOG_LEVEL_TOKENS[DEFAULT_LOG_LEVEL_ORDINAL]);

prt::LogLevel getLogLevel(const OP_Node* node, fpreal t);

} // namespace CommonNodeParams

namespace AssignNodeParams {

// -- PRIMITIVE CLASSIFIER NAME
static PRM_Name PRIM_CLS("primClsAttr", "Primitive Classifier");
const std::string PRIM_CLS_HELP = "Classifies primitives into input shapes and sets value for primitive attribute '" +
                                  PLD_PRIM_CLS_NAME.toStdString() + "'";
static PRM_Default PRIM_CLS_DEFAULT(0.0f, "primCls", CH_STRING_LITERAL);

const auto getPrimClsName = [](const OP_Node* node, fpreal t) -> UT_String {
	UT_String s;
	node->evalString(s, PRIM_CLS.getToken(), 0, t);
	return s;
};

const auto setPrimClsName = [](OP_Node* node, const UT_String& name, fpreal t) {
	node->setString(name, CH_STRING_LITERAL, PRIM_CLS.getToken(), 0, t);
};

// -- RULE PACKAGE
static PRM_Name RPK("rpk", "Rule Package");
const std::string RPK_HELP = "Sets value for primitive attribute '" + PLD_RPK.toStdString() + "'";

static PRM_Default RPK_DEFAULT(0, "");
int updateRPK(void* data, int index, fpreal32 time, const PRM_Template*);
static PRM_Callback rpkCallback(&updateRPK);

const auto getRPK = [](const OP_Node* node, fpreal t) -> PLD_BOOST_NS::filesystem::path {
	UT_String s;
	node->evalString(s, RPK.getToken(), 0, t);
	return s.toStdString();
};

// -- RPK RELOADER
static PRM_Name RPK_RELOAD("rpkReload", "Reload Rule Package");

// -- RULE FILE (cgb)
static PRM_Name RULE_FILE("ruleFile", "Rule File");
const std::string RULE_FILE_HELP = "Sets value for primitive attribute '" + PLD_RULE_FILE.toStdString() + "'";

void buildRuleFileMenu(void* data, PRM_Name* theMenu, int theMaxSize, const PRM_SpareData*, const PRM_Parm*);

static PRM_ChoiceList ruleFileMenu(static_cast<PRM_ChoiceListType>(PRM_CHOICELIST_REPLACE), &buildRuleFileMenu);

const auto getRuleFile = [](const OP_Node* node, fpreal t) -> std::wstring {
	UT_String s;
	node->evalString(s, RULE_FILE.getToken(), 0, t);
	return toUTF16FromOSNarrow(s.toStdString());
};

const auto setRuleFile = [](OP_Node* node, const std::wstring& ruleFile, fpreal t) {
	const UT_String val(toOSNarrowFromUTF16(ruleFile));
	node->setString(val, CH_STRING_LITERAL, RULE_FILE.getToken(), 0, t);
};

// -- STYLE
static PRM_Name STYLE("style", "Style");
const std::string STYLE_HELP = "Sets value for primitive attribute '" + PLD_STYLE.toStdString() + "'";

void buildStyleMenu(void* data, PRM_Name* theMenu, int theMaxSize, const PRM_SpareData*, const PRM_Parm*);

static PRM_ChoiceList styleMenu(static_cast<PRM_ChoiceListType>(PRM_CHOICELIST_REPLACE), &buildStyleMenu);

const auto getStyle = [](const OP_Node* node, fpreal t) -> std::wstring {
	UT_String s;
	node->evalString(s, STYLE.getToken(), 0, t);
	return toUTF16FromOSNarrow(s.toStdString());
};

const auto setStyle = [](OP_Node* node, const std::wstring& s, fpreal t) {
	const UT_String val(toOSNarrowFromUTF16(s));
	node->setString(val, CH_STRING_LITERAL, STYLE.getToken(), 0, t);
};

// -- START RULE
static PRM_Name START_RULE("startRule", "Start Rule");
const std::string START_RULE_HELP = "Sets value for primitive attribute '" + PLD_START_RULE.toStdString() + "'";

void buildStartRuleMenu(void* data, PRM_Name* theMenu, int theMaxSize, const PRM_SpareData*, const PRM_Parm*);

static PRM_ChoiceList startRuleMenu(static_cast<PRM_ChoiceListType>(PRM_CHOICELIST_REPLACE), &buildStartRuleMenu);

const auto getStartRule = [](const OP_Node* node, fpreal t) -> std::wstring {
	UT_String s;
	node->evalString(s, START_RULE.getToken(), 0, t);
	return toUTF16FromOSNarrow(s.toStdString());
};

const auto setStartRule = [](OP_Node* node, const std::wstring& s, fpreal t) {
	const UT_String val(toOSNarrowFromUTF16(s));
	node->setString(val, CH_STRING_LITERAL, START_RULE.getToken(), 0, t);
};

// -- OVERRIDABLE ATTRIBUTES
using AttributeValueType = PLD_BOOST_NS::variant<std::wstring, double, bool>;
using AttributeValueMap = std::map<std::wstring, AttributeValueType>;

const UT_String ATTRIBUTE_NONE = "(none)";
static PRM_Default ATTRIBUTE_DEFAULT(0.0, ATTRIBUTE_NONE.c_str());
static PRM_Name ATTRIBUTE("attribute#", "Attribute");
void buildAttributeMenu(void* data, PRM_Name* theMenu, int theMaxSize, const PRM_SpareData*, const PRM_Parm*);
int updateAttributeDefaultValue(void* data, int index, fpreal32 time, const PRM_Template*);
static PRM_Callback attributeCallback(&updateAttributeDefaultValue);
static PRM_ChoiceList attributeMenu(PRM_CHOICELIST_SINGLE, &buildAttributeMenu);
bool updateParmsFlags(SOPAssign& opParm, fpreal time);

static PRM_Name ATTRIBUTE_STRING_VALUE("stringValue#", "String Value");
static PRM_Name ATTRIBUTE_FLOAT_VALUE("floatValue#", "Number Value");
static PRM_Name ATTRIBUTE_BOOL_VALUE("boolValue#", "Boolean Value");

static PRM_Name ATTRIBUTES_OVERRIDE("attributesOverride", "Attribute Overrides");
static PRM_Template PARAM_ATTRIBUTE_TEMPLATE[] = {
        PRM_Template(PRM_STRING, 1, &ATTRIBUTE, &ATTRIBUTE_DEFAULT, &attributeMenu, nullptr, attributeCallback,
                     nullptr),
        PRM_Template(PRM_STRING, 1, &ATTRIBUTE_STRING_VALUE, PRMoneDefaults, nullptr, nullptr, PRM_Callback(), nullptr),
        PRM_Template(PRM_FLT, 1, &ATTRIBUTE_FLOAT_VALUE, PRMoneDefaults, nullptr, nullptr, PRM_Callback(), nullptr),
        PRM_Template(PRM_TOGGLE, 1, &ATTRIBUTE_BOOL_VALUE, PRMoneDefaults, nullptr, nullptr, PRM_Callback(), nullptr),
        PRM_Template()};

AttributeValueMap getOverriddenRuleAttributes(SOPAssign* node, fpreal32 time);

// -- ASSIGN NODE PARAMS
static PRM_Template PARAM_TEMPLATES[] = {
        PRM_Template(PRM_STRING, 1, &PRIM_CLS, &PRIM_CLS_DEFAULT, nullptr, nullptr, PRM_Callback(), nullptr, 1,
                     PRIM_CLS_HELP.c_str()),
        PRM_Template(PRM_FILE, 1, &RPK, &RPK_DEFAULT, nullptr, nullptr, rpkCallback,
                     &PRM_SpareData::fileChooserModeRead, 1, RPK_HELP.c_str()),
        PRM_Template(PRM_CALLBACK, 1, &RPK_RELOAD, PRMoneDefaults, nullptr, nullptr, rpkCallback),
        PRM_Template(PRM_STRING, 1, &RULE_FILE, PRMoneDefaults, &ruleFileMenu, nullptr, PRM_Callback(), nullptr, 1,
                     RULE_FILE_HELP.c_str()),
        PRM_Template(PRM_STRING, 1, &STYLE, PRMoneDefaults, &styleMenu, nullptr, PRM_Callback(), nullptr, 1,
                     STYLE_HELP.c_str()),
        PRM_Template(PRM_STRING, 1, &START_RULE, PRMoneDefaults, &startRuleMenu, nullptr, PRM_Callback(), nullptr, 1,
                     START_RULE_HELP.c_str()),
        PRM_Template(PRM_ORD, PRM_Template::PRM_EXPORT_MAX, 1, &CommonNodeParams::LOG_LEVEL,
                     &CommonNodeParams::DEFAULT_LOG_LEVEL, &CommonNodeParams::logLevelMenu),
        PRM_Template(PRM_MULTITYPE_LIST, PARAM_ATTRIBUTE_TEMPLATE, 0.0f, &ATTRIBUTES_OVERRIDE, nullptr, nullptr,
                     nullptr, nullptr, nullptr, attributeCallback),
        PRM_Template()};

} // namespace AssignNodeParams

namespace GenerateNodeParams {

static PRM_Name GROUP_CREATION("groupCreation", "Primitive Groups");
static const char* GROUP_CREATION_TOKENS[] = {"NONE", "PRIMCLS"};
static const char* GROUP_CREATION_LABELS[] = {"Do not create groups", "One group per primitive classifier"};
static PRM_Name GROUP_CREATION_MENU_ITEMS[] = {PRM_Name(GROUP_CREATION_TOKENS[0], GROUP_CREATION_LABELS[0]),
                                               PRM_Name(GROUP_CREATION_TOKENS[1], GROUP_CREATION_LABELS[1]),
                                               PRM_Name(nullptr)};
static PRM_ChoiceList groupCreationMenu((PRM_ChoiceListType)(PRM_CHOICELIST_EXCLUSIVE | PRM_CHOICELIST_REPLACE),
                                        GROUP_CREATION_MENU_ITEMS);
const size_t DEFAULT_GROUP_CREATION_ORDINAL = 0;
static PRM_Default DEFAULT_GROUP_CREATION(0, GROUP_CREATION_TOKENS[DEFAULT_GROUP_CREATION_ORDINAL]);

const auto getGroupCreation = [](const OP_Node* node, fpreal t) -> GroupCreation {
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

static PRM_Name EMIT_ATTRS("emitAttrs", "Re-emit set CGA attributes");
static PRM_Name EMIT_MATERIAL("emitMaterials", "Emit material attributes");
static PRM_Name EMIT_REPORTS("emitReports", "Emit CGA reports");
static PRM_Template PARAM_TEMPLATES[]{PRM_Template(PRM_ORD, PRM_Template::PRM_EXPORT_MAX, 1, &GROUP_CREATION,
                                                   &DEFAULT_GROUP_CREATION, &groupCreationMenu),
                                      PRM_Template(PRM_TOGGLE, 1, &EMIT_ATTRS),
                                      PRM_Template(PRM_TOGGLE, 1, &EMIT_MATERIAL),
                                      PRM_Template(PRM_TOGGLE, 1, &EMIT_REPORTS),
                                      PRM_Template(PRM_ORD, PRM_Template::PRM_EXPORT_MAX, 1,
                                                   &CommonNodeParams::LOG_LEVEL, &CommonNodeParams::DEFAULT_LOG_LEVEL,
                                                   &CommonNodeParams::logLevelMenu),
                                      PRM_Template()};

} // namespace GenerateNodeParams
