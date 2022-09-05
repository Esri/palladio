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

#include "NodeSpareParameter.h"
#include "AttributeConversion.h"
#include "SOPAssign.h"

#include "OP/OP_Director.h"
#include "PI/PI_EditScriptedParms.h"
#include "PRM/PRM_ChoiceList.h"
#include "PRM/PRM_SpareData.h"
#include "UT/UT_VarEncode.h"

namespace {
std::string getUniqueIdFromFolderVec(const FolderVec& parentFolders) {
	std::wstring folderString;

	for (const std::wstring& group : parentFolders) {
		if (folderString.empty())
			folderString += group;
		else
			folderString += NameConversion::GROUP_SEPARATOR + group;
	}

	UT_StringHolder primAttr(toOSNarrowFromUTF16(folderString).c_str());
	primAttr = UT_VarEncode::encodeAttrib(primAttr);

	return primAttr.toStdString();
}

void addMissingFolders(OP_Node* node, const FolderVec& parentFolders) {
	PI_EditScriptedParms nodeParms(node, 1, 0);
	if (!parentFolders.empty()) {
		const std::string uniqueFolderId = getUniqueIdFromFolderVec(parentFolders);
		const int folderIdx = nodeParms.getFolderIndexWithName(uniqueFolderId);
		if (folderIdx <= 0) {
			const FolderVec newParentFolders = (parentFolders.size() > 1)
			                                           ? FolderVec(parentFolders.begin(), parentFolders.end() - 1)
			                                           : FolderVec();
			addMissingFolders(node, newParentFolders);

			NodeSpareParameter::addCollapsibleFolder(node, parentFolders.back(), newParentFolders);
		}
	}
}

void addParmsFromTemplateArray(OP_Node* node, PRM_Template* spareParmTemplates, const FolderVec& parentFolders = {}) {
	UT_String errors;
	OP_Director* director = OPgetDirector();

	addMissingFolders(node, parentFolders);

	PI_EditScriptedParms nodeParms(node, 1, 0);
	const PI_EditScriptedParms spareParms(node, spareParmTemplates, 1, 0, 0);

	const std::string uniqueFolderId = getUniqueIdFromFolderVec(parentFolders);
	const int folderIdx = nodeParms.getFolderIndexWithName(uniqueFolderId);

	nodeParms.mergeParms(spareParms);

	if (folderIdx > 0) {
		const int folderEndIdx = nodeParms.getMatchingGroupParm(folderIdx);
		const int parmIdxlast = nodeParms.getNParms() - 1;
		const int matchingParmIdx = nodeParms.getMatchingGroupParm(parmIdxlast);
		const int parmIdxfirst = (matchingParmIdx != -1) ? matchingParmIdx : parmIdxlast;

		nodeParms.moveParms(parmIdxfirst, parmIdxlast, folderEndIdx - parmIdxfirst);
	}

	director->changeNodeSpareParms(node, nodeParms, errors);
}

void addParm(OP_Node* node, PRM_Type parmType, const std::wstring& id, const std::wstring& name, PRM_Default defaultVal,
             PRM_Range* range = nullptr, const FolderVec& parentFolders = {}, const std::wstring& description = {}) {
	UT_StringHolder token(toOSNarrowFromUTF16(id));
	UT_StringHolder label(toOSNarrowFromUTF16(name));

	std::string helpText(toOSNarrowFromUTF16(description));

	PRM_Name stringParmName(token, label);
	PRM_Template templateArr[] = {PRM_Template(parmType,          // parm type
	                                           1,                 // number of folders
	                                           &stringParmName,   // name
	                                           &defaultVal,       // defaults
	                                           nullptr,           // choicelist
	                                           range,             // rangeptr
	                                           nullptr,           // callbackfunc
	                                           nullptr,           // spareData
	                                           1,                 // parmGroup
	                                           helpText.c_str()), // helptext
	                              PRM_Template()};

	addParmsFromTemplateArray(node, templateArr, parentFolders);
}

template <typename T>
void addArrayParm(OP_Node* node, const std::wstring& id, const std::wstring& name, const std::vector<T>& defaultVals,
                  const FolderVec& parentFolders, const std::wstring& annotation, const char* itemNameToken,
                  PRM_Type itemType) {
	UT_StringHolder token(toOSNarrowFromUTF16(id));
	UT_StringHolder label(toOSNarrowFromUTF16(name));

	PRM_Name templateName(token, label);
	PRM_Name itemName(itemNameToken, "#");
	PRM_Default defaultVal(defaultVals.size());

	PRM_Template itemTemplate[] = {PRM_Template(itemType, 1, &itemName, PRMzeroDefaults), PRM_Template()};

	PRM_Template myTemplateList[] = {PRM_Template(PRM_MULTITYPE_LIST, itemTemplate, 0, &templateName, &defaultVal,
	                                              nullptr, &PRM_SpareData::multiStartOffsetZero),
	                                 PRM_Template()};

	addParmsFromTemplateArray(node, myTemplateList, parentFolders);
}

} // namespace

namespace NodeSpareParameter {

void addFloatParm(OP_Node* node, const std::wstring& id, const std::wstring& name, double defaultVal, double min,
                  double max, const FolderVec& parentFolders, const std::wstring& description) {
	PRM_Range range =
	        (!std::isnan(min) && !std::isnan(max)) ? PRM_Range(PRM_RANGE_UI, min, PRM_RANGE_UI, max) : PRM_Range();

	addParm(node, PRM_FLT, id, name, PRM_Default(defaultVal), &range, parentFolders, description);
}

void addColorParm(OP_Node* node, const std::wstring& id, const std::wstring& name, std::array<double, 3> defaultVal,
                  const FolderVec& parentFolders, const std::wstring& description) {
	UT_StringHolder token(toOSNarrowFromUTF16(id));
	UT_StringHolder label(toOSNarrowFromUTF16(name));

	std::string helpText(toOSNarrowFromUTF16(description));

	PRM_Default defaultVals[] = {PRM_Default(defaultVal[0]), PRM_Default(defaultVal[1]), PRM_Default(defaultVal[2])};

	PRM_Name stringParmName(token, label);

	PRM_Template templateArr[] = {PRM_Template(PRM_RGB,           // parm type
	                                           3,                 // number of folders
	                                           &stringParmName,   // name
	                                           defaultVals,       // defaults
	                                           nullptr,           // choicelist
	                                           nullptr,             // rangeptr
	                                           nullptr,           // callbackfunc
	                                           nullptr,           // spareData
	                                           1,                 // parmGroup
	                                           helpText.c_str()), // helptext
	                              PRM_Template()};

	addParmsFromTemplateArray(node, templateArr, parentFolders);
}

void addIntParm(OP_Node* node, const std::wstring& id, const std::wstring& name, int defaultVal, double min, double max,
                const FolderVec& parentFolders, const std::wstring& description) {
	PRM_Range range =
	        (!std::isnan(min) && !std::isnan(max)) ? PRM_Range(PRM_RANGE_UI, min, PRM_RANGE_UI, max) : PRM_Range();

	addParm(node, PRM_INT, id, name, PRM_Default(defaultVal), &range, parentFolders, description);
}

void addBoolParm(OP_Node* node, const std::wstring& id, const std::wstring& name, bool defaultVal,
                 const FolderVec& parentFolders, const std::wstring& description) {
	addParm(node, PRM_TOGGLE, id, name, PRM_Default(defaultVal), nullptr, parentFolders, description);
}

void addStringParm(OP_Node* node, const std::wstring& id, const std::wstring& name, const std::wstring& defaultVal,
                   const FolderVec& parentFolders, const std::wstring& description) {
	addParm(node, PRM_STRING, id, name, PRM_Default(0, toOSNarrowFromUTF16(defaultVal).c_str()), nullptr,
	        parentFolders, description);
}

void addBoolArrayParm(OP_Node* node, const std::wstring& id, const std::wstring& name,
                    const std::vector<bool>& defaultVals, const FolderVec& parentFolders,
                    const std::wstring& annotation) {
	addArrayParm<bool>(node, id, name, defaultVals, parentFolders, annotation, "boolVecParm#", PRM_TOGGLE);
}

void addFloatArrayParm(OP_Node* node, const std::wstring& id, const std::wstring& name,
                     const std::vector<double>& defaultVals, const FolderVec& parentFolders,
                     const std::wstring& annotation) {
	addArrayParm<double>(node, id, name, defaultVals, parentFolders, annotation, "floatVecParm#", PRM_FLT);
}

void addStringArrayParm(OP_Node* node, const std::wstring& id, const std::wstring& name,
                      const std::vector<std::wstring>& defaultVals, const FolderVec& parentFolders,
                      const std::wstring& annotation) {
	addArrayParm<std::wstring>(node, id, name, defaultVals, parentFolders, annotation, "stringVecParm#", PRM_STRING);
}

void addEnumParm(OP_Node* node, const std::wstring& id, const std::wstring& name, const std::wstring& defaultOption,
                 const std::vector<std::wstring>& mOptions, const FolderVec& parentFolders,
                 const std::wstring& description) {
	const size_t optionCount = mOptions.size();

	std::unique_ptr<PRM_Name[]> optionNames = std::make_unique<PRM_Name[]>(optionCount + 1);

	PRM_Default defaultVal(0);
	for (int i = 0; i < optionCount; ++i) {
		UT_StringHolder option(toOSNarrowFromUTF16(mOptions[i]));
		optionNames[i].setTokenAndLabel(option, option);
		if (mOptions[i] == defaultOption)
			defaultVal.setOrdinal(i);
	}
	optionNames[optionCount].setAsSentinel();

	PRM_ChoiceList enumMenu((PRM_ChoiceListType)(PRM_CHOICELIST_EXCLUSIVE | PRM_CHOICELIST_REPLACE), optionNames.get());

	UT_StringHolder token(toOSNarrowFromUTF16(id));
	UT_StringHolder label(toOSNarrowFromUTF16(name));

	std::string helpText(toOSNarrowFromUTF16(description));

	PRM_Name stringParmName(token, label);

	PRM_Template templateArr[] = {PRM_Template(PRM_ORD,           // parm type
	                                           PRM_Template::PRM_EXPORT_MAX, // number of folders
	                                           1,
                                               &stringParmName,   // name
	                                           &defaultVal,       // defaults
	                                           &enumMenu,         // choicelist
	                                           nullptr,           // rangeptr
	                                           nullptr,           // callbackfunc
	                                           nullptr,           // spareData
	                                           1,                 // parmGroup
	                                           helpText.c_str()), // helptext
	                              PRM_Template()};

	addParmsFromTemplateArray(node, templateArr, parentFolders);
}

void addFileParm(OP_Node* node, const std::wstring& id, const std::wstring& name, const std::wstring& defaultVal,
                 const FolderVec& parentFolders, const std::wstring& description) {
	addParm(node, PRM_FILE, id, name, PRM_Default(0, toOSNarrowFromUTF16(defaultVal).c_str()), nullptr, parentFolders, description);
}

void addDirectoryParm(OP_Node* node, const std::wstring& id, const std::wstring& name, const std::wstring& defaultVal,
                      const FolderVec& parentFolders, const std::wstring& description) {
	addParm(node, PRM_DIRECTORY, id, name, PRM_Default(0, toOSNarrowFromUTF16(defaultVal).c_str()), nullptr,
	        parentFolders, description);
}

void addSeparator(OP_Node* node, const FolderVec& parentFolders) {
	addParm(node, PRM_SEPARATOR, L"separator", L"separator", PRM_Default(), nullptr, parentFolders);
}

void addFolder(OP_Node* node, PRM_SpareData* groupType, const std::wstring& name, const FolderVec& parentFolders) {
	FolderVec newFolders = parentFolders;
	newFolders.push_back(name);

	const std::string uniqueFolderId = getUniqueIdFromFolderVec(newFolders);
	const UT_StringHolder token(uniqueFolderId);
	const UT_StringHolder label(toOSNarrowFromUTF16(name));

	PRM_Name folderName(token, label);
	PRM_Default folderList[] = {
	        PRM_Default(1, label) // 1 is number of parameters in tab
	};

	PRM_Template templateList[] = {PRM_Template(PRM_SWITCHER, // parm type
	                                            1,            // number of folders
	                                            &folderName,  // name
	                                            folderList,   // defaults
	                                            0,            // choicelist
	                                            0,            // rangeptr
	                                            0,            // callbackfunc
	                                            groupType),   // spareData
	                               PRM_Template()};

	addParmsFromTemplateArray(node, templateList, parentFolders);
}

void addSimpleFolder(OP_Node* node, const std::wstring& name, const FolderVec& parentFolders) {
	addFolder(node, &PRM_SpareData::groupTypeSimple, name, parentFolders);
}

void addCollapsibleFolder(OP_Node* node, const std::wstring& name, const FolderVec& parentFolders) {
	addFolder(node, &PRM_SpareData::groupTypeCollapsible, name, parentFolders);
}

void clearAllParms(OP_Node* node) {
	UT_String errors;
	OP_Director* director = OPgetDirector();

	PI_EditScriptedParms emptyParms = {};
	director->changeNodeSpareParms(node, emptyParms, errors);
}

} // namespace NodeSpareParameter