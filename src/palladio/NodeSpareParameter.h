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

#include "OP/OP_Node.h"

#include "PRM/PRM_Range.h"

using FolderVec = std::vector<std::wstring>;

namespace NodeSpareParameter {

void addParmsFromTemplateArray(OP_Node* node, PRM_Template* spareParmTemplates, const FolderVec& parentFolders = {});
void clearAllParms(OP_Node* node);
void addParm(OP_Node* node, PRM_Type parmType, const std::wstring& id, const std::wstring& name, PRM_Default defaultVal,
             PRM_Range* range = nullptr, const FolderVec& parentFolder = {});
void addFloatParm(OP_Node* node, const std::wstring& id, const std::wstring& name, double defaultVal,
                  double min = std::numeric_limits<double>::quiet_NaN(),
                  double max = std::numeric_limits<double>::quiet_NaN(), const FolderVec& parentFolders = {});
void addIntParm(OP_Node* node, const std::wstring& id, const std::wstring& name, int defaultVal,
                double min = std::numeric_limits<double>::quiet_NaN(),
                double max = std::numeric_limits<double>::quiet_NaN(), const FolderVec& parentFolders = {});
void addBoolParm(OP_Node* node, const std::wstring& id, const std::wstring& name, bool defaultVal,
                 const FolderVec& parentFolders = {});
void addStringParm(OP_Node* node, const std::wstring& id, const std::wstring& name, const std::wstring& defaultVal,
                   const FolderVec& parentFolders = {});
void addEnumParm(OP_Node* node, const std::wstring& id, const std::wstring& name, const std::wstring& defaultIdx,
                 const std::vector<std::wstring>& mOptions, const FolderVec& parentFolders);
void addSeparator(OP_Node* node, const FolderVec& parentFolders = {});
void addFolder(OP_Node* node, PRM_SpareData* groupType, const std::wstring& name, const FolderVec& parentFolders = {});
void addSimpleFolder(OP_Node* node, const std::wstring& name, const FolderVec& parentFolders = {});
void addCollapsibleFolder(OP_Node* node, const std::wstring& name, const FolderVec& parentFolders = {});
} // namespace NodeSpareParameter