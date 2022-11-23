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
constexpr const char* PRM_SPARE_IS_PERCENT_TOKEN = "palladio::is_percent";

constexpr const char* PRM_ENUM_TYPE_TOKEN = "palladio::enum_type";
constexpr const char* PRM_ENUM_TYPE_STRING = "string";
constexpr const char* PRM_ENUM_TYPE_FLOAT = "float";

void clearAllParms(OP_Node* node);
void addFloatParm(OP_Node* node, const std::wstring& id, const std::wstring& name, double defaultVal,
                  double min = std::numeric_limits<double>::quiet_NaN(),
                  double max = std::numeric_limits<double>::quiet_NaN(), bool restricted = true, bool isPercent = false,
                  const FolderVec& parentFolders = {}, const std::wstring& description = {});
void addColorParm(OP_Node* node, const std::wstring& id, const std::wstring& name, std::array<double, 3> defaultVal,
                  const FolderVec& parentFolders, const std::wstring& description = {});
void addBoolParm(OP_Node* node, const std::wstring& id, const std::wstring& name, bool defaultVal,
                 const FolderVec& parentFolders = {}, const std::wstring& description = {});
void addStringParm(OP_Node* node, const std::wstring& id, const std::wstring& name, const std::wstring& defaultVal,
                   const FolderVec& parentFolders = {}, const std::wstring& description = {});
void addBoolArrayParm(OP_Node* node, const std::wstring& id, const std::wstring& name,
                      const std::vector<bool>& defaultVals, const FolderVec& parentFolders = {},
                      const std::wstring& annotation = {});
void addFloatArrayParm(OP_Node* node, const std::wstring& id, const std::wstring& name,
                       const std::vector<double>& defaultVals, const FolderVec& parentFolders = {},
                       const std::wstring& annotation = {});
void addStringArrayParm(OP_Node* node, const std::wstring& id, const std::wstring& name,
                        const std::vector<std::wstring>& defaultVals, const FolderVec& parentFolders = {},
                        const std::wstring& annotation = {});
void addStringEnumParm(OP_Node* node, const std::wstring& id, const std::wstring& name,
                       const std::wstring& defaultOption, const std::vector<std::wstring>& mOptions,
                       const FolderVec& parentFolders, const std::wstring& description = {});
void addFloatEnumParm(OP_Node* node, const std::wstring& id, const std::wstring& name,
                      const std::wstring& defaultOption, const std::vector<std::wstring>& mOptions,
                      const FolderVec& parentFolders, const std::wstring& description = {});
void addFileParm(OP_Node* node, const std::wstring& id, const std::wstring& name, const std::wstring& defaultVal,
                 const std::vector<std::wstring>& extensions, const FolderVec& parentFolders, const std::wstring& description = {});
void addDirectoryParm(OP_Node* node, const std::wstring& id, const std::wstring& name, const std::wstring& defaultVal,
                      const FolderVec& parentFolders, const std::wstring& description = {});
void addSeparator(OP_Node* node, const FolderVec& parentFolders = {});
void addFolder(OP_Node* node, PRM_SpareData* groupType, const std::wstring& name, const FolderVec& parentFolders = {});
void addSimpleFolder(OP_Node* node, const std::wstring& name, const FolderVec& parentFolders = {});
void addCollapsibleFolder(OP_Node* node, const std::wstring& name, const FolderVec& parentFolders = {});
} // namespace NodeSpareParameter