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

namespace LogLevel {

static PRM_Name NAME("logLevel", "Log Level");
static const char* TOKENS[] = {"Default", "Debug", "Info", "Warning", "Error", "Fatal", "None"};
static PRM_Name MENU_ITEMS[] = {PRM_Name(TOKENS[0]), PRM_Name(TOKENS[1]), PRM_Name(TOKENS[2]), PRM_Name(TOKENS[3]),
                                PRM_Name(TOKENS[4]), PRM_Name(TOKENS[5]), PRM_Name(TOKENS[6]), PRM_Name(nullptr)};
static PRM_ChoiceList buildMenu((PRM_ChoiceListType)(PRM_CHOICELIST_EXCLUSIVE | PRM_CHOICELIST_REPLACE), MENU_ITEMS);
const size_t DEFAULT_ORDINAL = 0;
static PRM_Default DEFAULT(0, TOKENS[DEFAULT_ORDINAL]);

static PRM_Template TEMPLATE(PRM_ORD, PRM_Template::PRM_EXPORT_MAX, 1, &NAME, &DEFAULT, &buildMenu);

prt::LogLevel getLogLevel(const OP_Node* node, fpreal t);

} // namespace LogLevel

} // namespace CommonNodeParams

namespace AssignNodeParams {

namespace PrimitiveClassifierName {

static PRM_Name NAME("primClsAttr", "Primitive Classifier");
static PRM_Default DEFAULT(0.0f, "primCls", CH_STRING_LITERAL);
const std::string HELP = "Classifies primitives into input shapes and sets value for primitive attribute '" +
                         PLD_PRIM_CLS_NAME.toStdString() + "'";

static PRM_Template TEMPLATE(PRM_STRING, 1, &NAME, &DEFAULT, nullptr, nullptr, PRM_Callback(), nullptr, 1,
                             HELP.c_str());

UT_String getPrimClsName(const OP_Node* node, fpreal t);
void setPrimClsName(OP_Node* node, const UT_String& name, fpreal t);

} // namespace PrimitiveClassifierName

namespace RulePackage {

static PRM_Name NAME("rpk", "Rule Package");
static PRM_Default DEFAULT(0, "");
const std::string HELP = "Sets value for primitive attribute '" + PLD_RPK.toStdString() + "'";

int updateRPK(void* data, int index, fpreal32 time, const PRM_Template*);
static PRM_Callback rpkCallback(&updateRPK);

static PRM_Template TEMPLATE(PRM_FILE, 1, &NAME, &DEFAULT, nullptr, nullptr, rpkCallback,
                             &PRM_SpareData::fileChooserModeRead, 1, HELP.c_str());

PLD_BOOST_NS::filesystem::path getRPK(const OP_Node* node, fpreal t);

} // namespace RulePackage

namespace RulePackageReload {

static PRM_Name NAME("rpkReload", "Reload Rule Package");
static PRM_Template TEMPLATE(PRM_CALLBACK, 1, &NAME, PRMoneDefaults, nullptr, nullptr, RulePackage::rpkCallback);

} // namespace RulePackageReload

namespace RuleFile {

static PRM_Name NAME("ruleFile", "Rule File");
const std::string HELP = "Sets value for primitive attribute '" + PLD_RULE_FILE.toStdString() + "'";

void buildRuleFileMenu(void* data, PRM_Name* theMenu, int theMaxSize, const PRM_SpareData*, const PRM_Parm*);
static PRM_ChoiceList buildMenu(static_cast<PRM_ChoiceListType>(PRM_CHOICELIST_REPLACE), &buildRuleFileMenu);

static PRM_Template TEMPLATE(PRM_STRING, 1, &NAME, PRMoneDefaults, &buildMenu, nullptr, PRM_Callback(), nullptr, 1,
                             HELP.c_str());

std::wstring getRuleFile(const OP_Node* node, fpreal t);
void setRuleFile(OP_Node* node, const std::wstring& ruleFile, fpreal t);

} // namespace RuleFile

namespace Style {
static PRM_Name NAME("style", "Style");
const std::string HELP = "Sets value for primitive attribute '" + PLD_STYLE.toStdString() + "'";

void buildMenu(void* data, PRM_Name* theMenu, int theMaxSize, const PRM_SpareData*, const PRM_Parm*);
static PRM_ChoiceList MENU(static_cast<PRM_ChoiceListType>(PRM_CHOICELIST_REPLACE), &buildMenu);

static PRM_Template TEMPLATE(PRM_STRING, 1, &NAME, PRMoneDefaults, &MENU, nullptr, PRM_Callback(), nullptr, 1,
                             HELP.c_str());

std::wstring getStyle(const OP_Node* node, fpreal t);
void setStyle(OP_Node* node, const std::wstring& s, fpreal t);

} // namespace Style

namespace StartRule {

static PRM_Name NAME("startRule", "Start Rule");
const std::string HELP = "Sets value for primitive attribute '" + PLD_START_RULE.toStdString() + "'";

void buildMenu(void* data, PRM_Name* theMenu, int theMaxSize, const PRM_SpareData*, const PRM_Parm*);
static PRM_ChoiceList MENU(static_cast<PRM_ChoiceListType>(PRM_CHOICELIST_REPLACE), &buildMenu);

static PRM_Template TEMPLATE(PRM_STRING, 1, &NAME, PRMoneDefaults, &MENU, nullptr, PRM_Callback(), nullptr, 1,
                             HELP.c_str());

std::wstring getStartRule(const OP_Node* node, fpreal t);
void setStartRule(OP_Node* node, const std::wstring& s, fpreal t);

} // namespace StartRule

namespace AttributeOverrides {

using AttributeValueType = PLD_BOOST_NS::variant<std::wstring, double, bool>;
using AttributeValueMap = std::map<std::wstring, AttributeValueType>;

const UT_String ATTRIBUTE_NONE = "(none)";
static PRM_Default ATTRIBUTE_DEFAULT(0.0, ATTRIBUTE_NONE.c_str());
static PRM_Name ATTRIBUTE_NAME("attribute#", "Attribute");
int updateAttributeDefaultValue(void* data, int index, fpreal32 time, const PRM_Template*);
static PRM_Callback attributeCallback(&updateAttributeDefaultValue);
void buildAttributeMenu(void* data, PRM_Name* theMenu, int theMaxSize, const PRM_SpareData*, const PRM_Parm*);
static PRM_ChoiceList attributeMenu(PRM_CHOICELIST_SINGLE, &buildAttributeMenu);
static PRM_Template ATTRIBUTE_TEMPLATE(PRM_STRING | PRM_TYPE_JOIN_NEXT, 1, &ATTRIBUTE_NAME, &ATTRIBUTE_DEFAULT,
                                       &attributeMenu, nullptr, attributeCallback, nullptr);

static PRM_Name STRING_NAME("stringValue#", "String Value");
static PRM_Template STRING_TEMPLATE(PRM_STRING, 1, &STRING_NAME, PRMoneDefaults, nullptr, nullptr, PRM_Callback(),
                                    nullptr);

static PRM_Name FLOAT_NAME("floatValue#", "Number Value");
static PRM_Template FLOAT_TEMPLATE(PRM_FLT, 1, &FLOAT_NAME, PRMoneDefaults, nullptr, nullptr, PRM_Callback(), nullptr);

static PRM_Name BOOL_NAME("boolValue#", "Boolean Value");
static PRM_Template BOOL_TEMPLATE(PRM_TOGGLE, 1, &BOOL_NAME, PRMoneDefaults, nullptr, nullptr, PRM_Callback(), nullptr);

int resetAttribute(void* data, int index, fpreal32 time, const PRM_Template*);
static PRM_Callback resetCallback(&resetAttribute);
static PRM_Name RESET_NAME("resetValue#", "Reset Value");
static PRM_Template RESET_TEMPLATE(PRM_CALLBACK, 1, &RESET_NAME, PRMoneDefaults, nullptr, nullptr, resetCallback);

static PRM_Name ATTRIBUTES_OVERRIDE("attributesOverride", "Attribute Overrides");
static PRM_Template TEMPLATES[] = {ATTRIBUTE_TEMPLATE, RESET_TEMPLATE, STRING_TEMPLATE,
                                   FLOAT_TEMPLATE,     BOOL_TEMPLATE,  PRM_Template()};

static PRM_Template MULTI_PARM_TEMPLATE(PRM_MULTITYPE_LIST, TEMPLATES, 0.0f, &ATTRIBUTES_OVERRIDE, nullptr, nullptr,
                                        nullptr, nullptr, nullptr, attributeCallback);

// helper functions for attribute overriding
AttributeValueMap getOverriddenRuleAttributes(SOPAssign* node, fpreal32 time);
bool updateParmsFlags(SOPAssign& opParm, fpreal time);

} // namespace AttributeOverrides

static PRM_Template PARAM_TEMPLATES[] = {PrimitiveClassifierName::TEMPLATE,
                                         RulePackage::TEMPLATE,
                                         RulePackageReload::TEMPLATE,
                                         RuleFile::TEMPLATE,
                                         Style::TEMPLATE,
                                         StartRule::TEMPLATE,
                                         CommonNodeParams::LogLevel::TEMPLATE,
                                         AttributeOverrides::MULTI_PARM_TEMPLATE,
                                         PRM_Template()};

} // namespace AssignNodeParams

namespace GenerateNodeParams {

namespace Group {

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

static PRM_Template TEMPLATE(PRM_ORD, PRM_Template::PRM_EXPORT_MAX, 1, &GROUP_CREATION, &DEFAULT_GROUP_CREATION,
                             &groupCreationMenu);
} // namespace Group

namespace EmitAttributes {

static PRM_Name NAME("emitAttrs", "Re-emit set CGA attributes");
static PRM_Template TEMPLATE(PRM_TOGGLE, 1, &NAME);

} // namespace EmitAttributes

namespace EmitMaterial {

static PRM_Name NAME("emitMaterials", "Emit material attributes");
static PRM_Template TEMPLATE(PRM_TOGGLE, 1, &NAME);

} // namespace EmitMaterial

namespace EmitReports {

static PRM_Name NAME("emitReports", "Emit CGA reports");
static PRM_Template TEMPLATE(PRM_TOGGLE, 1, &NAME);

} // namespace EmitReports

static PRM_Template PARAM_TEMPLATES[] = {Group::TEMPLATE,
                                         EmitAttributes::TEMPLATE,
                                         EmitMaterial::TEMPLATE,
                                         EmitReports::TEMPLATE,
                                         CommonNodeParams::LogLevel::TEMPLATE,
                                         PRM_Template()};

} // namespace GenerateNodeParams
