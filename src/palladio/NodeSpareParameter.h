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

#include "AnnotationParsing.h"
#include "RuleAttributes.h"

#include "OP/OP_Node.h"
#include "PRM/PRM_Range.h"
#include "GU/GU_Detail.h"

using FolderVec = std::vector<std::wstring>;

namespace NodeSpareParameter {

void addParmsFromTemplateArray(OP_Node* node, PRM_Template* spareParmTemplates, const FolderVec& parentFolders = {});
void clearAllParms(OP_Node* node);
void addParm(OP_Node* node, PRM_Type parmType, const std::wstring& id, const std::wstring& name, PRM_Default defaultVal,
             PRM_Range* range = nullptr, const FolderVec& parentFolders = {}, const std::wstring& description = {});
void addFloatParm(OP_Node* node, const std::wstring& id, const std::wstring& name, double defaultVal,
                  double min = std::numeric_limits<double>::quiet_NaN(),
                  double max = std::numeric_limits<double>::quiet_NaN(), const FolderVec& parentFolders = {},
                  const std::wstring& description = {});
void addColorParm(OP_Node* node, const std::wstring& id, const std::wstring& name, std::array<double, 3> defaultVal,
                  const FolderVec& parentFolders, const std::wstring& description = {});
void addIntParm(OP_Node* node, const std::wstring& id, const std::wstring& name, int defaultVal,
                double min = std::numeric_limits<double>::quiet_NaN(),
                double max = std::numeric_limits<double>::quiet_NaN(), const FolderVec& parentFolders = {},
                const std::wstring& description = {});
void addBoolParm(OP_Node* node, const std::wstring& id, const std::wstring& name, bool defaultVal,
                 const FolderVec& parentFolders = {}, const std::wstring& description = {});
void addStringParm(OP_Node* node, const std::wstring& id, const std::wstring& name, const std::wstring& defaultVal,
                   const FolderVec& parentFolders = {}, const std::wstring& description = {});
void addEnumParm(OP_Node* node, const std::wstring& id, const std::wstring& name, const std::wstring& defaultIdx,
                 const std::vector<std::wstring>& mOptions, const FolderVec& parentFolders,
                 const std::wstring& description = {});
void addFileParm(OP_Node* node, const std::wstring& id, const std::wstring& name, const std::wstring& defaultVal,
                 const FolderVec& parentFolders, const std::wstring& description = {});
void addDirectoryParm(OP_Node* node, const std::wstring& id, const std::wstring& name, const std::wstring& defaultVal,
                      const FolderVec& parentFolders, const std::wstring& description = {});
void addSeparator(OP_Node* node, const FolderVec& parentFolders = {});
void addFolder(OP_Node* node, PRM_SpareData* groupType, const std::wstring& name, const FolderVec& parentFolders = {});
void addSimpleFolder(OP_Node* node, const std::wstring& name, const FolderVec& parentFolders = {});
void addCollapsibleFolder(OP_Node* node, const std::wstring& name, const FolderVec& parentFolders = {});

enum class ParmType {
	STRING,
	DOUBLE,
	BOOL,
	STRING_ARRAY,
	DOUBLE_ARRAY,
	BOOL_ARRAY,
	STRING_ENUM,
	DOUBLE_ENUM,
	FILE,
	DIR,
	COLOR,
	ANGLE,
	PERCENT,
	NONE
};

using ParmDefaultValue =
        std::variant<std::wstring, double, bool, std::vector<std::wstring>, std::vector<double>, std::vector<bool>>;

class INodeSpareParm {
public:
	INodeSpareParm(std::wstring tokenId, std::wstring fqName, std::wstring name, ParmDefaultValue defVal,
	               FolderVec parentFolders = {},
	               std::wstring description = {})
	    : tokenId(tokenId), fqName(fqName), name(name), defVal(defVal), parentFolders(parentFolders), description(description) {};
	virtual void addToNode(OP_Node* node) = 0;
	void replaceDefaultValueFromMap(OP_Node* node, const AttributeMapVector& defaultRuleAttributeMaps);
	void addPrimitiveAttributeToNode(OP_Node* node, GU_Detail* detail);
	void addCurrentValueToAttrMap(OP_Node* node, AttributeMapBuilderUPtr& amb);
		

protected:
	virtual void addCurrentValueToAttrMap(OP_Node* node, AttributeMapBuilderUPtr& amb, PRM_Parm* parmPtr,
	                                      double time) = 0;
	virtual void addPrimitiveAttributeToNode(OP_Node* node, GU_Detail* detail, PRM_Parm* parmPtr, double time) = 0;
	virtual void setCurrentDefaultValue(OP_Node* node, PRM_Parm* parmPtr, double time) = 0;

public:
	ParmType type = ParmType::NONE;
	std::wstring tokenId;

protected:
	std::wstring name;
	std::wstring fqName;
	ParmDefaultValue defVal;
	std::wstring description;
	FolderVec parentFolders;
};

class StringSpareParm : public INodeSpareParm {
public:
	StringSpareParm(std::wstring tokenId, std::wstring fqName, std::wstring name, std::wstring defVal, FolderVec parentFolders = {},
	                std::wstring description = {})
	    : INodeSpareParm(tokenId, fqName, name, defVal, parentFolders, description) {
		type = ParmType::STRING;
	}
	void addToNode(OP_Node* node) override;

protected:
	void addCurrentValueToAttrMap(OP_Node* node, AttributeMapBuilderUPtr& amb, PRM_Parm* parmPtr,
	                                      double time) override;
	void addPrimitiveAttributeToNode(OP_Node* node, GU_Detail* detail, PRM_Parm* parmPtr, double time) override;
	void setCurrentDefaultValue(OP_Node* node, PRM_Parm* parmPtr, double time) override;
	std::wstring getDefVal() {
		return std::get<std::wstring>(defVal);
	}
};

class FloatSpareParm : public INodeSpareParm {
public:
	FloatSpareParm(std::wstring tokenId, std::wstring fqName, std::wstring name, double defVal, double min = 0, double max = 10,
	               FolderVec parentFolders = {}, std::wstring description = {})
	    : INodeSpareParm(tokenId, fqName, name, defVal, parentFolders, description), min(min), max(max) {
		type = ParmType::DOUBLE;
	}
	void addToNode(OP_Node* node) override;

protected:
	void addCurrentValueToAttrMap(OP_Node* node, AttributeMapBuilderUPtr& amb, PRM_Parm* parmPtr, double time) override;
	void setCurrentDefaultValue(OP_Node* node, PRM_Parm* parmPtr, double time) override;
	void addPrimitiveAttributeToNode(OP_Node* node, GU_Detail* detail, PRM_Parm* parmPtr, double time) override;
	double getDefVal() {
		return std::get<double>(defVal);
	}

protected:
	double min;
	double max;
};

class PercentSpareParm : public FloatSpareParm {
public:
	PercentSpareParm(std::wstring tokenId, std::wstring fqName, std::wstring name, double defVal, FolderVec parentFolders = {},
	                 std::wstring description = {})
	    : FloatSpareParm(tokenId, fqName, name, defVal, 0, 1, parentFolders, description) {
		type = ParmType::PERCENT;
	}
};

class AngleSpareParm : public FloatSpareParm {
public:
	// diplay % values with a 0-1 range for now (avoid scaling by 100)
	AngleSpareParm(std::wstring tokenId, std::wstring fqName, std::wstring name, double defVal, FolderVec parentFolders = {},
	               std::wstring description = {})
	    : FloatSpareParm(tokenId, fqName, name, defVal, 0, 1, parentFolders, description) {
		type = ParmType::ANGLE;
	}
};

class BoolSpareParm : public INodeSpareParm {
public:
	BoolSpareParm(std::wstring tokenId, std::wstring fqName, std::wstring name, bool defVal, FolderVec parentFolders = {}, std::wstring description = {})
	    : INodeSpareParm(tokenId, fqName, name, defVal, parentFolders, description) {
		type = ParmType::BOOL;
	}

	void addToNode(OP_Node* node) override;

protected:
	void addCurrentValueToAttrMap(OP_Node* node, AttributeMapBuilderUPtr& amb, PRM_Parm* parmPtr, double time) override;
	void setCurrentDefaultValue(OP_Node* node, PRM_Parm* parmPtr, double time) override;
	void addPrimitiveAttributeToNode(OP_Node* node, GU_Detail* detail, PRM_Parm* parmPtr, double time) override;
	bool getDefVal() {
		return std::get<bool>(defVal);
	}
};

class StringEnumSpareParm : public INodeSpareParm {
public:
	StringEnumSpareParm(std::wstring tokenId, std::wstring fqName, std::wstring name, std::wstring defVal,
	                    AnnotationParsing::EnumAnnotation enumAnnotation = {}, FolderVec parentFolders = {},
	                    std::wstring description = {})
	    : INodeSpareParm(tokenId, fqName, name, defVal, parentFolders, description),
	      enumAnnotation(enumAnnotation) {
		type = ParmType::STRING_ENUM;
	}

	void addToNode(OP_Node* node) override;

protected:
	void addCurrentValueToAttrMap(OP_Node* node, AttributeMapBuilderUPtr& amb, PRM_Parm* parmPtr, double time) override;
	void setCurrentDefaultValue(OP_Node* node, PRM_Parm* parmPtr, double time) override;
	void addPrimitiveAttributeToNode(OP_Node* node, GU_Detail* detail, PRM_Parm* parmPtr, double time) override;


protected:
	AnnotationParsing::EnumAnnotation enumAnnotation;
	std::wstring getDefVal() {
		return std::get<std::wstring>(defVal);
	}
};

class FloatEnumSpareParm : public INodeSpareParm {
public:
	FloatEnumSpareParm(std::wstring tokenId, std::wstring fqName, std::wstring name, double defVal,
	                   AnnotationParsing::EnumAnnotation enumAnnotation = {}, FolderVec parentFolders = {},
	                   std::wstring description = {})
	    : INodeSpareParm(tokenId, fqName, name, defVal, parentFolders, description), enumAnnotation(enumAnnotation) {
		type = ParmType::DOUBLE_ENUM;
	}

	void addToNode(OP_Node* node) override;

protected:
	void addCurrentValueToAttrMap(OP_Node* node, AttributeMapBuilderUPtr& amb, PRM_Parm* parmPtr, double time) override;
	void setCurrentDefaultValue(OP_Node* node, PRM_Parm* parmPtr, double time) override;
	void addPrimitiveAttributeToNode(OP_Node* node, GU_Detail* detail, PRM_Parm* parmPtr, double time) override;
	double getDefVal() {
		return std::get<double>(defVal);
	}

protected:
	AnnotationParsing::EnumAnnotation enumAnnotation;
};

class FileSpareParm : public INodeSpareParm {
public:
	FileSpareParm(std::wstring tokenId, std::wstring fqName, std::wstring name, std::wstring defVal, FolderVec parentFolders = {},
	              std::wstring description = {})
	    : INodeSpareParm(tokenId, fqName, name, defVal, parentFolders, description) {
		type = ParmType::FILE;
	}

	void addToNode(OP_Node* node) override;

protected:
	void addCurrentValueToAttrMap(OP_Node* node, AttributeMapBuilderUPtr& amb, PRM_Parm* parmPtr, double time) override;
	void setCurrentDefaultValue(OP_Node* node, PRM_Parm* parmPtr, double time) override;
	void addPrimitiveAttributeToNode(OP_Node* node, GU_Detail* detail, PRM_Parm* parmPtr, double time) override;
	std::wstring getDefVal() {
		return std::get<std::wstring>(defVal);
	}

protected:
	std::vector<std::wstring> fileExtensions;
};

class DirSpareParm : public INodeSpareParm {
public:
	DirSpareParm(std::wstring tokenId, std::wstring fqName, std::wstring name, std::wstring defVal, FolderVec parentFolders = {},
	             std::wstring description = {})
	    : INodeSpareParm(tokenId, fqName, name, defVal, parentFolders, description) {
		type = ParmType::DIR;
	}

	void addToNode(OP_Node* node) override;

protected:
	void addCurrentValueToAttrMap(OP_Node* node, AttributeMapBuilderUPtr& amb, PRM_Parm* parmPtr, double time) override;
	void setCurrentDefaultValue(OP_Node* node, PRM_Parm* parmPtr, double time) override;
	void addPrimitiveAttributeToNode(OP_Node* node, GU_Detail* detail, PRM_Parm* parmPtr, double time) override;
	std::wstring getDefVal() {
		return std::get<std::wstring>(defVal);
	}
};

class ColorSpareParm : public INodeSpareParm {
public:
	ColorSpareParm(std::wstring tokenId, std::wstring fqName, std::wstring name, std::wstring defVal, FolderVec parentFolders = {},
	               std::wstring description = {})
	    : INodeSpareParm(tokenId, fqName, name, defVal, parentFolders, description) {
		type = ParmType::COLOR;
	}

	void addToNode(OP_Node* node) override;

protected:
	void addCurrentValueToAttrMap(OP_Node* node, AttributeMapBuilderUPtr& amb, PRM_Parm* parmPtr, double time) override;
	void setCurrentDefaultValue(OP_Node* node, PRM_Parm* parmPtr, double time) override;
	void addPrimitiveAttributeToNode(OP_Node* node, GU_Detail* detail, PRM_Parm* parmPtr, double time) override;
	std::wstring getDefVal() {
		return std::get<std::wstring>(defVal);
	}
};

constexpr const wchar_t* RULE_ATTRIBUTES_FOLDER_NAME = L"Rule Attributes";

using SpareParmSPtr = std::shared_ptr<NodeSpareParameter::INodeSpareParm>;
using SpareParmVec = std::vector<SpareParmSPtr>;
SpareParmVec createSpareParmsFromAttrInfo(const AttributeMapVector& defaultRuleAttributeMaps,
                                            const RuleFileInfoUPtr& ruleFileInfo,
                                            const RuleAttributeSet& ruleAttributes);
} // namespace NodeSpareParameter