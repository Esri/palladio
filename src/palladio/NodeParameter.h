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
#include "PRM/PRM_Range.h"
#include "PRM/PRM_Shared.h"
#include "PRM/PRM_SpareData.h"

#include "prt/LogLevel.h"

#include <filesystem>

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

UT_String getPrimClsName(const OP_Node* node, fpreal t);
void setPrimClsName(OP_Node* node, const UT_String& name, fpreal t);

// -- RULE PACKAGE
static PRM_Name RPK("rpk", "Rule Package");
const std::string RPK_HELP = "Sets value for primitive attribute '" + PLD_RPK.toStdString() + "'";

static PRM_Default RPK_DEFAULT(0, "");
int updateRPK(void* data, int index, fpreal32 time, const PRM_Template*);
static PRM_Callback rpkCallback(&updateRPK);

std::filesystem::path getRPK(const OP_Node* node, fpreal t);

// -- RPK RELOADER
static PRM_Name RPK_RELOAD("rpkReload", "Reload Rule Package");

// -- STYLE
static PRM_Name STYLE("style", "Style");
const std::string STYLE_HELP = "Sets value for primitive attribute '" + PLD_STYLE.toStdString() + "'";

void buildStyleMenu(void* data, PRM_Name* theMenu, int theMaxSize, const PRM_SpareData*, const PRM_Parm*);

static PRM_ChoiceList styleMenu(static_cast<PRM_ChoiceListType>(PRM_CHOICELIST_REPLACE), &buildStyleMenu);

std::wstring getStyle(const OP_Node* node, fpreal t);

void setStyle(OP_Node* node, const std::wstring& s, fpreal t);

// -- START RULE
static PRM_Name START_RULE("startRule", "Start Rule");
const std::string START_RULE_HELP = "Sets value for primitive attribute '" + PLD_START_RULE.toStdString() + "'";

void buildStartRuleMenu(void* data, PRM_Name* theMenu, int theMaxSize, const PRM_SpareData*, const PRM_Parm*);

static PRM_ChoiceList startRuleMenu(static_cast<PRM_ChoiceListType>(PRM_CHOICELIST_REPLACE), &buildStartRuleMenu);

std::wstring getStartRule(const OP_Node* node, fpreal t);
void setStartRule(OP_Node* node, const std::wstring& s, fpreal t);

// -- SEED
static PRM_Name SEED("randomSeed", "Seed");
static PRM_Range SEED_RANGE(PRM_RANGE_RESTRICTED, std::numeric_limits<int>::lowest(), PRM_RANGE_RESTRICTED,
                            std::numeric_limits<int>::max());
static PRM_Name OVERRIDE_SEED("overrideRandomSeed", "Override");
const std::string SEED_HELP = "Sets the random seed for all input primitives (attribute '" +
                              PLD_RANDOM_SEED.toStdString() +
                              "'). This value is ignored if the primitive attribute already exists and \"override\" is "
                              "not checked. By default, the random seed is derived from the primitive position.";
const std::string OVERRIDE_SEED_HELP =
        "Force setting the random seed even if the corresponding primitive attribute already exists.";

int getSeed(const OP_Node* node, fpreal t);
bool getOverrideSeed(const OP_Node* node, fpreal t);

static PRM_Name GENERATE_NEW_SEED("generateNewSeed", "Generate new Seed");
int generateNewSeed(void* data, int, fpreal32 time, const PRM_Template*);
static PRM_Callback generateNewSeedCallback(&generateNewSeed);

// -- ASSIGN NODE PARAMS
static PRM_Template PARAM_TEMPLATES[] = {
        PRM_Template(PRM_STRING, 1, &PRIM_CLS, &PRIM_CLS_DEFAULT, nullptr, nullptr, PRM_Callback(), nullptr, 1,
                     PRIM_CLS_HELP.c_str()),
        PRM_Template(PRM_FILE, 1, &RPK, &RPK_DEFAULT, nullptr, nullptr, rpkCallback,
                     &PRM_SpareData::fileChooserModeRead, 1, RPK_HELP.c_str()),
        PRM_Template(PRM_CALLBACK, 1, &RPK_RELOAD, PRMoneDefaults, nullptr, nullptr, rpkCallback),
        PRM_Template(PRM_STRING, 1, &STYLE, PRMoneDefaults, &styleMenu, nullptr, PRM_Callback(), nullptr, 1,
                     STYLE_HELP.c_str()),
        PRM_Template(PRM_STRING, 1, &START_RULE, PRMoneDefaults, &startRuleMenu, nullptr, PRM_Callback(), nullptr, 1,
                     START_RULE_HELP.c_str()),
        PRM_Template(PRM_ORD, PRM_Template::PRM_EXPORT_MAX, 1, &CommonNodeParams::LOG_LEVEL,
                     &CommonNodeParams::DEFAULT_LOG_LEVEL, &CommonNodeParams::logLevelMenu),
        PRM_Template(PRM_INT | PRM_TYPE_JOIN_NEXT | PRM_TYPE_PLAIN, 1, &SEED, PRMzeroDefaults, nullptr, &SEED_RANGE,
                     PRM_Callback(), nullptr, 1, SEED_HELP.c_str()),
        PRM_Template(PRM_TOGGLE, 1, &OVERRIDE_SEED, PRMzeroDefaults, nullptr, nullptr, PRM_Callback(), nullptr, 1,
                     OVERRIDE_SEED_HELP.c_str()),
        PRM_Template(PRM_CALLBACK, 1, &GENERATE_NEW_SEED, PRMoneDefaults, nullptr, nullptr, generateNewSeedCallback),
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

GroupCreation getGroupCreation(const OP_Node* node, fpreal t);

static PRM_Name EMIT_ATTRS("emitAttrs", "Re-emit set CGA attributes");
static PRM_Name EMIT_MATERIAL("emitMaterials", "Emit material attributes");
static PRM_Name EMIT_REPORTS("emitReports", "Emit CGA reports");
static PRM_Name TRIANGULATE_FACES_WITH_HOLES("triangulateFacesWithHoles", "Triangulate polygons with holes");
static PRM_Template PARAM_TEMPLATES[]{PRM_Template(PRM_ORD, PRM_Template::PRM_EXPORT_MAX, 1, &GROUP_CREATION,
                                                   &DEFAULT_GROUP_CREATION, &groupCreationMenu),
                                      PRM_Template(PRM_TOGGLE, 1, &EMIT_ATTRS),
                                      PRM_Template(PRM_TOGGLE, 1, &EMIT_MATERIAL),
                                      PRM_Template(PRM_TOGGLE, 1, &EMIT_REPORTS),
                                      PRM_Template(PRM_TOGGLE, 1, &TRIANGULATE_FACES_WITH_HOLES, PRMoneDefaults),
                                      PRM_Template(PRM_ORD, PRM_Template::PRM_EXPORT_MAX, 1,
                                                   &CommonNodeParams::LOG_LEVEL, &CommonNodeParams::DEFAULT_LOG_LEVEL,
                                                   &CommonNodeParams::logLevelMenu),
                                      PRM_Template()};

} // namespace GenerateNodeParams
