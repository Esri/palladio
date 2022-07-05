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

#include "PRM/PRM_SpareData.h"
#include "OP/OP_Director.h"
#include "PI/PI_EditScriptedParms.h"
#include "SOPAssign.h"

#include "BoostRedirect.h"
#include PLD_BOOST_INCLUDE(/functional/hash.hpp)

namespace {
const std::wstring parameterPrefix = L"_palladioAttribute_";

static PRM_Range freeRange = PRM_Range(PRM_RANGE_FREE, 0, PRM_RANGE_FREE);

std::wstring getUniqueIdFromFolderVec(const folderVec& parentFolders) {
	std::hash<std::wstring> stringHasher;
	size_t folderHash = 0;
	
	for (const std::wstring& group : parentFolders) {
		PLD_BOOST_NS::hash_combine(folderHash, group);
	}
	return parameterPrefix + std::to_wstring(folderHash);
}
}

namespace NodeSpareParameter {

void addMissingFolders(OP_Node* node, const folderVec& parentFolders) {
	PI_EditScriptedParms nodeParms(node, 1, 0);
	if (!parentFolders.empty()) {
		const std::wstring uniqueFolderId = getUniqueIdFromFolderVec(parentFolders);
		const int folderIdx = nodeParms.getFolderIndexWithName(toOSNarrowFromUTF16(uniqueFolderId));
		if (folderIdx <= 0) {
			const folderVec newParentFolders =
			        (parentFolders.size() > 1) ? folderVec(parentFolders.begin(), parentFolders.end() - 1) : folderVec();
			addMissingFolders(node, newParentFolders);

			addCollapsibleFolder(node, parentFolders.back(), newParentFolders);
		}
	}
}

void addParmsFromTemplateArray(OP_Node* node, PRM_Template* spareParmTemplates, const folderVec& parentFolders) {
	UT_String errors;
	OP_Director* director = OPgetDirector();

	addMissingFolders(node, parentFolders);

	PI_EditScriptedParms nodeParms(node, 1, 0);
	const PI_EditScriptedParms spareParms(node, spareParmTemplates, 1, 0, 0);

	const std::wstring uniqueFolderId = getUniqueIdFromFolderVec(parentFolders);
	const int folderIdx = nodeParms.getFolderIndexWithName(toOSNarrowFromUTF16(uniqueFolderId));
	
	nodeParms.mergeParms(spareParms);

	if (folderIdx > 0) {
		const int folderEndIdx = nodeParms.getMatchingGroupParm(folderIdx);
		const int myParmIdx = nodeParms.getNParms() - 1;
		nodeParms.moveParms(myParmIdx, myParmIdx, folderEndIdx - myParmIdx);
	}

	director->changeNodeSpareParms(node, nodeParms, errors);
}

void addParm(OP_Node* node, PRM_Type parmType, const std::wstring& id, const std::wstring& name,
             PRM_Default defaultVal, PRM_Range* range, const folderVec& parentFolders) {
	UT_StringHolder token(toOSNarrowFromUTF16(id));
	UT_StringHolder label(toOSNarrowFromUTF16(name));

	PRM_Name stringParmName(token, label);
	PRM_Template templateArr[] = {
		PRM_Template(parmType, 
		             1, 
					 &stringParmName, 
					 &defaultVal,
					 nullptr,
					 range),
		PRM_Template()
	};

	addParmsFromTemplateArray(node, templateArr, parentFolders);
}

void addFloatParm(OP_Node* node, const std::wstring& id, const std::wstring& name, double defaultVal, double min,
                  double max, const folderVec& parentFolders) {
	PRM_Range range =
	        (!std::isnan(min) && !std::isnan(max)) ? PRM_Range(PRM_RANGE_UI, min, PRM_RANGE_UI, max) : PRM_Range();

	addParm(node, PRM_FLT, id, name, PRM_Default(defaultVal), &range, parentFolders);
}

void addIntParm(OP_Node* node, const std::wstring& id, const std::wstring& name, int defaultVal, double min, double max,
                const folderVec& parentFolders) {
	PRM_Range range =
	        (!std::isnan(min) && !std::isnan(max)) ? PRM_Range(PRM_RANGE_UI, min, PRM_RANGE_UI, max) : PRM_Range();

	addParm(node, PRM_INT, id, name, PRM_Default(defaultVal), &range, parentFolders);
}

void addBoolParm(OP_Node* node, const std::wstring& id, const std::wstring& name, bool defaultVal, const folderVec& parentFolders) {
	addParm(node, PRM_TOGGLE, id, name, PRM_Default(defaultVal), nullptr, parentFolders);
}

void addStringParm(OP_Node* node, const std::wstring& id, const std::wstring& name, const std::wstring& defaultVal,
                   const folderVec& parentFolders) {
	addParm(node, PRM_STRING, id, name, PRM_Default(0, toOSNarrowFromUTF16(defaultVal).c_str()), nullptr,
	        parentFolders);
}

void addSeparator(OP_Node* node, const folderVec& parentFolders) {
	addParm(node, PRM_SEPARATOR, L"separator", L"separator", PRM_Default(), nullptr, parentFolders);
}

void addFolder(OP_Node* node, PRM_SpareData* groupType, const std::wstring& name,
               const folderVec& parentFolders) {
	folderVec newFolders = parentFolders;
	newFolders.push_back(name);

	const std::wstring uniqueFolderId = getUniqueIdFromFolderVec(newFolders);
	const UT_StringHolder token(toOSNarrowFromUTF16(uniqueFolderId));
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

void addSimpleFolder(OP_Node* node, const std::wstring& name, const folderVec& parentFolders) {
	addFolder(node, &PRM_SpareData::groupTypeSimple, name, parentFolders);
}

void addCollapsibleFolder(OP_Node* node, const std::wstring& name, const folderVec& parentFolders) {
	addFolder(node, &PRM_SpareData::groupTypeCollapsible, name, parentFolders);
}

void clearAllParms(OP_Node* node) {
	UT_String errors;
	OP_Director* director = OPgetDirector();

	PI_EditScriptedParms emptyParms = {};
	director->changeNodeSpareParms(node, emptyParms, errors);
}

}