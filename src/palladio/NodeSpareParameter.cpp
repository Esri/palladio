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

NodeSpareParameter::ParmDefaultValue getDefValFromAttrMap(const std::wstring& key,
                                                          const AttributeMapVector& attributeMaps) {
	NodeSpareParameter::ParmDefaultValue defVal;
	bool defValAssigned;
	for (size_t mapIdx = 0; mapIdx < attributeMaps.size(); mapIdx++) {
		const auto& defaultRuleAttributes = attributeMaps[mapIdx];

		if (!defaultRuleAttributes->hasKey(key.c_str()))
			continue;

		const auto type = defaultRuleAttributes->getType(key.c_str());

		switch (type) {
			case prt::AttributeMap::PT_FLOAT: {
				defVal = defaultRuleAttributes->getFloat(key.c_str());
				defValAssigned = true;
				break;
			}
			case prt::AttributeMap::PT_BOOL: {
				defVal = defaultRuleAttributes->getBool(key.c_str());
				defValAssigned = true;
				break;
			}
			case prt::AttributeMap::PT_STRING: {
				const wchar_t* v = defaultRuleAttributes->getString(key.c_str());
				assert(v != nullptr);
				defVal = std::wstring(v);
				assert(defVal.index() == 0); // std::wstring is type index 0
				defValAssigned = true;
				break;
			}
			default:
				continue;
		}
		if (defValAssigned)
			break;
	}
	return defVal;
}
} // namespace

namespace NodeSpareParameter {

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

			addCollapsibleFolder(node, parentFolders.back(), newParentFolders);
		}
	}
}

void addParmsFromTemplateArray(OP_Node* node, PRM_Template* spareParmTemplates, const FolderVec& parentFolders) {
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
		const int myParmIdx = nodeParms.getNParms() - 1;
		nodeParms.moveParms(myParmIdx, myParmIdx, folderEndIdx - myParmIdx);
	}

	director->changeNodeSpareParms(node, nodeParms, errors);
}

void addParm(OP_Node* node, PRM_Type parmType, const std::wstring& id, const std::wstring& name, PRM_Default defaultVal,
             PRM_Range* range, const FolderVec& parentFolders, const std::wstring& description) {
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



// NodeSpare parm Interface
void INodeSpareParm::addCurrentValueToAttrMap(OP_Node* node, AttributeMapBuilderUPtr& amb) {
	const fpreal time = CHgetEvalTime();
	const UT_StringHolder parmId(toOSNarrowFromUTF16(tokenId));
	PRM_Parm* parmPtr = node->getParmPtr(parmId);
	if (!parmPtr->isDefault())
		addCurrentValueToAttrMap(node, amb, parmPtr, time);
}

void INodeSpareParm::addPrimitiveAttributeToNode(OP_Node* node, GU_Detail* detail) {
	const fpreal time = CHgetEvalTime();
	const UT_StringHolder parmId(toOSNarrowFromUTF16(tokenId));
	PRM_Parm* parmPtr = node->getParmPtr(parmId);
	if (!parmPtr->isDefault())
		addPrimitiveAttributeToNode(node, detail, parmPtr, time);
}

void INodeSpareParm::replaceDefaultValueFromMap(OP_Node* node, const AttributeMapVector& defaultRuleAttributeMaps) {
	defVal = getDefValFromAttrMap(fqName, defaultRuleAttributeMaps);

	const fpreal time = CHgetEvalTime();

	const UT_StringHolder parmId(toOSNarrowFromUTF16(tokenId));
	// Very delicate code! using node->getParm() causes crashes.
	// Unsure why switching to pointers seems to work..
	PRM_Parm* parmPtr = node->getParmPtr(parmId);
	if (!parmPtr->isDefault())
		return;
	setCurrentDefaultValue(node, parmPtr, time);
	parmPtr->overwriteDefaults(time);

	PRM_Template* templatePtr = parmPtr->getTemplatePtr();

	parmPtr->overwriteDefaults(time);
	if (!parmPtr->isFactoryDefaultUI()) {
		PRM_Template* templatePtr = parmPtr->getTemplatePtr();
		PRM_Default* factoryDefaults = templatePtr->getFactoryDefaults();

		for (size_t idx = 0; idx < templatePtr->getVectorSize(); ++idx) {
			PRM_Default* defaultValue = templatePtr->getDefault(idx);

			factoryDefaults[idx].set(defaultValue->getFloat(), defaultValue->getString(),
			                         defaultValue->getStringMeaning());
		}
	}
}


// String parm
void StringSpareParm::addToNode(OP_Node* node) {
	NodeSpareParameter::addStringParm(node, tokenId, name, getDefVal(), parentFolders, description);
}

void StringSpareParm::addCurrentValueToAttrMap(OP_Node* node, AttributeMapBuilderUPtr& amb, PRM_Parm* parmPtr,
                                               double time) {
	UT_String stringValue;
	node->evalString(stringValue, parmPtr, 0, time);
	const std::wstring wstringValue = toUTF16FromOSNarrow(stringValue.toStdString());

	amb->setString(fqName.c_str(), wstringValue.c_str());
}

void StringSpareParm::addPrimitiveAttributeToNode(OP_Node* node, GU_Detail* detail, PRM_Parm* parmPtr, double time) {
	UT_String stringValue;
	node->evalString(stringValue, parmPtr, 0, time);

	GA_RWHandleS stringHandle(detail->addStringTuple(GA_AttributeOwner::GA_ATTRIB_PRIMITIVE, parmPtr->getToken(), 1));
	stringHandle.set(0, stringValue);
}

void StringSpareParm::setCurrentDefaultValue(OP_Node* node, PRM_Parm* parmPtr, double time) {
	const UT_StringHolder stringValue(toOSNarrowFromUTF16(getDefVal()));
	node->setString(stringValue, CH_StringMeaning::CH_STRING_LITERAL, parmPtr->getToken(), 0, time);
}


// Float parm
void FloatSpareParm::addToNode(OP_Node* node) {
	NodeSpareParameter::addFloatParm(node, tokenId, name, getDefVal(), min, max, parentFolders, description);
}

void FloatSpareParm::addCurrentValueToAttrMap(OP_Node* node, AttributeMapBuilderUPtr& amb, PRM_Parm* parmPtr, double time) {
	const double floatValue = node->evalFloat(parmPtr, 0, time);
	amb->setFloat(fqName.c_str(), floatValue);
}

void FloatSpareParm::addPrimitiveAttributeToNode(OP_Node* node, GU_Detail* detail, PRM_Parm* parmPtr, double time) {
	const double floatValue = node->evalFloat(parmPtr, 0, time);

	GA_RWHandleD floatHandle(detail->addFloatTuple(GA_AttributeOwner::GA_ATTRIB_PRIMITIVE, parmPtr->getToken(), 1));
	floatHandle.set(0, floatValue);
}

void FloatSpareParm::setCurrentDefaultValue(OP_Node* node, PRM_Parm* parmPtr, double time) {
	node->setFloat(parmPtr->getToken(), 0, time, getDefVal());
}


// Bool parm
void BoolSpareParm::addToNode(OP_Node* node) {
	NodeSpareParameter::addBoolParm(node, tokenId, name, getDefVal(), parentFolders, description);
}

void BoolSpareParm::addCurrentValueToAttrMap(OP_Node* node, AttributeMapBuilderUPtr& amb, PRM_Parm* parmPtr, double time) {
	const int intValue = node->evalInt(parmPtr, 0, time);
	amb->setBool(fqName.c_str(), static_cast<bool>(intValue));
}

void BoolSpareParm::addPrimitiveAttributeToNode(OP_Node* node, GU_Detail* detail, PRM_Parm* parmPtr, double time) {
	const int intValue = node->evalInt(parmPtr, 0, time);
	GA_RWHandleI ordinalHandle(detail->addIntTuple(GA_AttributeOwner::GA_ATTRIB_PRIMITIVE, parmPtr->getToken(), 1,
	                                               GA_Defaults(0), nullptr, nullptr, GA_STORE_INT8));
	ordinalHandle.set(0, intValue);
}

void BoolSpareParm::setCurrentDefaultValue(OP_Node* node, PRM_Parm* parmPtr, double time) {
	node->setInt(parmPtr->getToken(), 0, 0, getDefVal());
}


// StringEnum parm
void StringEnumSpareParm::addToNode(OP_Node* node) {
	NodeSpareParameter::addEnumParm(node, tokenId, name, getDefVal(), enumAnnotation.mOptions, parentFolders,
	                                description);
}

void StringEnumSpareParm::addCurrentValueToAttrMap(OP_Node* node, AttributeMapBuilderUPtr& amb, PRM_Parm* parmPtr, double time) {
	const int intValue = node->evalInt(parmPtr, 0, time);
	const PRM_ChoiceList* choices = parmPtr->getTemplatePtr()->getChoiceListPtr();
	if (choices == nullptr)
		return;
	UT_String result;
	if (!choices->tokenFromIndex(result, intValue))
		return;

	const std::wstring wstringValue = toUTF16FromOSNarrow(result.toStdString());
	amb->setString(fqName.c_str(), wstringValue.c_str());
}

void StringEnumSpareParm::addPrimitiveAttributeToNode(OP_Node* node, GU_Detail* detail,
                                                      PRM_Parm* parmPtr, double time) {
	const int intValue = node->evalInt(parmPtr, 0, time);
	const PRM_ChoiceList* choices = parmPtr->getTemplatePtr()->getChoiceListPtr();
	if (choices != nullptr) {
		UT_String result;
		if (choices->tokenFromIndex(result, intValue)) {
			GA_RWHandleS stringHandle(
			        detail->addStringTuple(GA_AttributeOwner::GA_ATTRIB_PRIMITIVE, parmPtr->getToken(), 1));
			stringHandle.set(0, result);
		}
	}
}
void StringEnumSpareParm::setCurrentDefaultValue(OP_Node* node, PRM_Parm* parmPtr, double time) {
	const PRM_ChoiceList* choices = parmPtr->getTemplatePtr()->getChoiceListPtr();
	if (choices != nullptr) {
		const PRM_Name* choiceNames;
		choices->getChoiceNames(choiceNames);
		for (int32 choiceIdx = 0; choiceIdx < choices->getSize(parmPtr); ++choiceIdx) {
			const std::string currEnumString = choiceNames[choiceIdx].getToken();
			if (currEnumString == toOSNarrowFromUTF16(getDefVal())) {
				node->setInt(parmPtr->getToken(), 0, time, choiceIdx);
				return;
			}
		}
	}
}


// FloatEnum parm
void FloatEnumSpareParm::addToNode(OP_Node* node) {
	const std::wstring fltEnumDefVal = std::to_wstring(getDefVal());
	NodeSpareParameter::addEnumParm(node, tokenId, name, fltEnumDefVal, enumAnnotation.mOptions, parentFolders, description);
}

void FloatEnumSpareParm::addCurrentValueToAttrMap(OP_Node* node, AttributeMapBuilderUPtr& amb, PRM_Parm* parmPtr, double time) {
	const int intValue = node->evalInt(parmPtr, 0, time);
	const PRM_ChoiceList* choices = parmPtr->getTemplatePtr()->getChoiceListPtr();
	if (choices == nullptr)
		return;
	UT_String result;
	if (!choices->tokenFromIndex(result, intValue))
		return;

	const double floatValue = result.toFloat();
	amb->setFloat(fqName.c_str(), floatValue);
}

void FloatEnumSpareParm::addPrimitiveAttributeToNode(OP_Node* node, GU_Detail* detail, PRM_Parm* parmPtr, double time) {
	const int intValue = node->evalInt(parmPtr, 0, time);
	const PRM_ChoiceList* choices = parmPtr->getTemplatePtr()->getChoiceListPtr();
	if (choices != nullptr) {
		UT_String result;
		if (choices->tokenFromIndex(result, intValue)) {
			GA_RWHandleS stringHandle(
			        detail->addStringTuple(GA_AttributeOwner::GA_ATTRIB_PRIMITIVE, parmPtr->getToken(), 1));
			stringHandle.set(0, result);
		}
	}
}

void FloatEnumSpareParm::setCurrentDefaultValue(OP_Node* node, PRM_Parm* parmPtr, double time) {
	const std::wstring wStringValue = std::to_wstring(getDefVal());
	const PRM_ChoiceList* choices = parmPtr->getTemplatePtr()->getChoiceListPtr();
	if (choices != nullptr) {
		const PRM_Name* choiceNames;
		choices->getChoiceNames(choiceNames);
		for (int32 choiceIdx = 0; choiceIdx < choices->getSize(parmPtr); ++choiceIdx) {
			const std::string currEnumString = choiceNames[choiceIdx].getToken();
			if (currEnumString == toOSNarrowFromUTF16(wStringValue)) {
				node->setInt(parmPtr->getToken(), 0, time, choiceIdx);
				return;
			}
		}
	}
}


// File parm
void FileSpareParm::addToNode(OP_Node* node) {
	NodeSpareParameter::addFileParm(node, tokenId, name, getDefVal(), parentFolders, description);
}

void FileSpareParm::addCurrentValueToAttrMap(OP_Node* node, AttributeMapBuilderUPtr& amb, PRM_Parm* parmPtr, double time) {
	UT_String stringValue;
	node->evalString(stringValue, parmPtr, 0, time);
	const std::wstring wstringValue = toUTF16FromOSNarrow(stringValue.toStdString());

	amb->setString(fqName.c_str(), wstringValue.c_str());
}

void FileSpareParm::addPrimitiveAttributeToNode(OP_Node* node, GU_Detail* detail, PRM_Parm* parmPtr, double time) {
	UT_String stringValue;
	node->evalString(stringValue, parmPtr, 0, time);

	GA_RWHandleS stringHandle(detail->addStringTuple(GA_AttributeOwner::GA_ATTRIB_PRIMITIVE, parmPtr->getToken(), 1));
	stringHandle.set(0, stringValue);
}

void FileSpareParm::setCurrentDefaultValue(OP_Node* node, PRM_Parm* parmPtr, double time) {
	const UT_StringHolder stringValue(toOSNarrowFromUTF16(getDefVal()));
	node->setString(stringValue, CH_StringMeaning::CH_STRING_LITERAL, parmPtr->getToken(), 0, time);
}


// Dir parm
void DirSpareParm::addToNode(OP_Node* node) {
	NodeSpareParameter::addDirectoryParm(node, tokenId, name, getDefVal(), parentFolders, description);
}

void DirSpareParm::addCurrentValueToAttrMap(OP_Node* node, AttributeMapBuilderUPtr& amb, PRM_Parm* parmPtr,
                                            double time) {
	UT_String stringValue;
	node->evalString(stringValue, parmPtr, 0, time);
	const std::wstring wstringValue = toUTF16FromOSNarrow(stringValue.toStdString());

	amb->setString(fqName.c_str(), wstringValue.c_str());
}

void DirSpareParm::addPrimitiveAttributeToNode(OP_Node* node, GU_Detail* detail, PRM_Parm* parmPtr, double time) {
	UT_String stringValue;
	node->evalString(stringValue, parmPtr, 0, time);

	GA_RWHandleS stringHandle(detail->addStringTuple(GA_AttributeOwner::GA_ATTRIB_PRIMITIVE, parmPtr->getToken(), 1));
	stringHandle.set(0, stringValue);
}

void DirSpareParm::setCurrentDefaultValue(OP_Node* node, PRM_Parm* parmPtr, double time) {
	const UT_StringHolder stringValue(toOSNarrowFromUTF16(getDefVal()));
	node->setString(stringValue, CH_StringMeaning::CH_STRING_LITERAL, parmPtr->getToken(), 0, time);
}


// Color parm
void ColorSpareParm::addToNode(OP_Node* node) {
	AnnotationParsing::ColorAnnotation color = AnnotationParsing::parseColor(getDefVal());
	NodeSpareParameter::addColorParm(node, tokenId, name, color, parentFolders, description);
}

void ColorSpareParm::addCurrentValueToAttrMap(OP_Node* node, AttributeMapBuilderUPtr& amb, PRM_Parm* parmPtr, double time) {
	const float r = node->evalFloat(parmPtr, 0, time);
	const float g = node->evalFloat(parmPtr, 1, time);
	const float b = node->evalFloat(parmPtr, 2, time);
	const std::wstring colorString = AnnotationParsing::getColorString({r, g, b});

	amb->setString(fqName.c_str(), colorString.c_str());
}

void ColorSpareParm::addPrimitiveAttributeToNode(OP_Node* node, GU_Detail* detail, PRM_Parm* parmPtr, double time) {
	const float r = node->evalFloat(parmPtr, 0, time);
	const float g = node->evalFloat(parmPtr, 1, time);
	const float b = node->evalFloat(parmPtr, 2, time);
	const std::wstring colorString = AnnotationParsing::getColorString({r, g, b});

	UT_String stringValue(toOSNarrowFromUTF16(colorString));
	GA_RWHandleS stringHandle(detail->addStringTuple(GA_AttributeOwner::GA_ATTRIB_PRIMITIVE, parmPtr->getToken(), 1));
	stringHandle.set(0, stringValue);
}


void ColorSpareParm::setCurrentDefaultValue(OP_Node* node, PRM_Parm* parmPtr, double time) {
	const auto color = AnnotationParsing::parseColor(getDefVal());

	node->setFloat(parmPtr->getToken(), 0, time, color[0]);
	node->setFloat(parmPtr->getToken(), 1, time, color[1]);
	node->setFloat(parmPtr->getToken(), 2, time, color[2]);
}

SpareParmVec createSpareParmsFromAttrInfo(const AttributeMapVector& defaultRuleAttributeMaps,
                                            const RuleFileInfoUPtr& ruleFileInfo,
                                            const RuleAttributeSet& ruleAttributes) {
	SpareParmVec spareParms;

	for (const auto& ra : ruleAttributes) {
		const std::wstring fqName = ra.fqName;
		NodeSpareParameter::ParmDefaultValue defVal = getDefValFromAttrMap(fqName, defaultRuleAttributeMaps);
		std::wstring niceName = ra.niceName;
		std::wstring tokenId = toUTF16FromOSNarrow(NameConversion::toPrimAttr(ra.fqName).toStdString());

		FolderVec parentFolders;
		parentFolders.push_back(RULE_ATTRIBUTES_FOLDER_NAME);
		parentFolders.push_back(ra.ruleFile);
		parentFolders.insert(parentFolders.end(), ra.groups.begin(), ra.groups.end());

		const auto annotationInfo = AnnotationParsing::getAttributeAnnotationInfo(ra.fqName, ruleFileInfo);
		std::wstring description = annotationInfo.mDescription;

		switch (ra.mType) {
			case prt::AnnotationArgumentType::AAT_BOOL: {
				const bool boolDefVal = std::get<bool>(defVal);
				spareParms.emplace_back(std::make_shared<NodeSpareParameter::BoolSpareParm>(
				        tokenId, fqName, niceName, boolDefVal, parentFolders, description));
				break;
			}
			case prt::AnnotationArgumentType::AAT_FLOAT: {
				const double doubleDefVal = std::get<double>(defVal);

				switch (annotationInfo.mAttributeTrait) {
					case AnnotationParsing::AttributeTrait::ENUM: {
						const auto enumAnnotation =
						        AnnotationParsing::parseEnumAnnotation(annotationInfo.mAnnotation);

						spareParms.emplace_back(std::make_shared<NodeSpareParameter::FloatEnumSpareParm>(
						        tokenId, fqName, niceName, doubleDefVal, enumAnnotation, parentFolders, description));
						break;
					}
					case AnnotationParsing::AttributeTrait::RANGE: {
						const auto minMax = AnnotationParsing::parseRangeAnnotation(annotationInfo.mAnnotation);
						spareParms.emplace_back(std::make_shared<NodeSpareParameter::FloatSpareParm>(
						        tokenId, fqName, niceName, doubleDefVal, minMax.first, minMax.second, parentFolders,
						        description));
						break;
					}
					case AnnotationParsing::AttributeTrait::ANGLE: {
						spareParms.emplace_back(std::make_shared<NodeSpareParameter::AngleSpareParm>(
						        tokenId, fqName, niceName, doubleDefVal, parentFolders, description));
						break;
					}
					case AnnotationParsing::AttributeTrait::PERCENT: {
						spareParms.emplace_back(std::make_shared<NodeSpareParameter::PercentSpareParm>(
						        tokenId, fqName, niceName, doubleDefVal, parentFolders, description));
						break;
					}
					default: {
						spareParms.emplace_back(std::make_shared<NodeSpareParameter::FloatSpareParm>(
						        tokenId, fqName, niceName, doubleDefVal, 0, 10, parentFolders, description));
						break;
					}
				}
				break;
			}
			case prt::AnnotationArgumentType::AAT_STR: {
				const std::wstring stringDefVal = std::get<std::wstring>(defVal);

				switch (annotationInfo.mAttributeTrait) {
					case AnnotationParsing::AttributeTrait::ENUM: {
						const auto enumAnnotation =
						        AnnotationParsing::parseEnumAnnotation(annotationInfo.mAnnotation);
						spareParms.emplace_back(std::make_shared<NodeSpareParameter::StringEnumSpareParm>(
						        tokenId, fqName, niceName, stringDefVal, enumAnnotation, parentFolders, description));
						break;
					}
					case AnnotationParsing::AttributeTrait::FILE: {
						spareParms.emplace_back(std::make_shared<NodeSpareParameter::FileSpareParm>(
						        tokenId, fqName, niceName, stringDefVal, parentFolders, description));
						break;
					}
					case AnnotationParsing::AttributeTrait::DIR: {
						spareParms.emplace_back(std::make_shared<NodeSpareParameter::DirSpareParm>(
						        tokenId, fqName, niceName, stringDefVal, parentFolders, description));
						break;
					}
					case AnnotationParsing::AttributeTrait::COLOR: {
						spareParms.emplace_back(std::make_shared<NodeSpareParameter::ColorSpareParm>(
						        tokenId, fqName, niceName, stringDefVal, parentFolders, description));
						break;
					}
					default: {
						spareParms.emplace_back(std::make_shared<NodeSpareParameter::StringSpareParm>(
						        tokenId, fqName, niceName, stringDefVal, parentFolders, description));
						break;
					}
				}
				break;
			}
			default:
				break;
		}
	
	}
	return spareParms;
}

} // namespace NodeSpareParameter