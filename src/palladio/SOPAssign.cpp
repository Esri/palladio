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

#include "SOPAssign.h"
#include "AnnotationParsing.h"
#include "AttrEvalCallbacks.h"
#include "AttributeConversion.h"
#include "LogHandler.h"
#include "ModelConverter.h"
#include "MultiWatch.h"
#include "NodeParameter.h"
#include "NodeSpareParameter.h"
#include "PrimitiveClassifier.h"
#include "RuleAttributes.h"
#include "ShapeData.h"
#include "ShapeGenerator.h"

#include "prt/API.h"

#include <numeric>

#include "CH/CH_Manager.h"
#include "GEO/GEO_AttributeHandle.h"
#include "UT/UT_Interrupt.h"

namespace {

constexpr bool DBG = false;
constexpr const wchar_t* ENCODER_ID_CGA_EVALATTR = L"com.esri.prt.core.AttributeEvalEncoder";

constexpr const wchar_t* RULE_ATTRIBUTES_FOLDER_NAME = L"Rule Attributes";

const double PERCENT_FACTOR = 100.0;

AttributeMapUPtr getValidEncoderInfo(const wchar_t* encID) {
	const EncoderInfoUPtr encInfo(prt::createEncoderInfo(encID));
	const prt::AttributeMap* encOpts = nullptr;
	encInfo->createValidatedOptionsAndStates(nullptr, &encOpts);
	return AttributeMapUPtr(encOpts);
}

RuleFileInfoUPtr getRuleFileInfo(const MainAttributes& ma, const ResolveMapSPtr& resolveMap, prt::Cache* prtCache) {
	std::vector<std::pair<std::wstring, std::wstring>> cgbs; // key -> uri
	getCGBs(resolveMap, cgbs);
	if (cgbs.empty())
		return {};

	const std::wstring cgbURI = cgbs.front().second;

	if (cgbURI.empty())
		return {};

	prt::Status status = prt::STATUS_UNSPECIFIED_ERROR;
	RuleFileInfoUPtr rfi(prt::createRuleFileInfo(cgbURI.c_str(), prtCache, &status));
	if (status != prt::STATUS_OK)
		return {};

	return rfi;
}

bool compareAttributeTypes(const SOPAssign::CGAAttributeValueMap& refDefaultValues,
                           const SOPAssign::CGAAttributeValueMap& newDefaultValues) {
	if (refDefaultValues.size() != newDefaultValues.size())
		return false;

	for (const auto& attributeValuePair : refDefaultValues) {
		const auto& it = newDefaultValues.find(attributeValuePair.first);

		if (it == newDefaultValues.end())
			return false;

		// check if attributes have the same data type
		if (it->second.index() != attributeValuePair.second.index())
			return false;
	}
	return true;
}

uint32 getVariantArraySize(const SOPAssign::CGAAttributeValueType& valArray) {
	if (std::holds_alternative<std::vector<bool>>(valArray))
		return std::get<std::vector<bool>>(valArray).size();
	else if (std::holds_alternative<std::vector<double>>(valArray))
		return std::get<std::vector<double>>(valArray).size();
	else if (std::holds_alternative<std::vector<std::wstring>>(valArray))
		return std::get<std::vector<std::wstring>>(valArray).size();

	return 0;
}

bool isMultiParmDefault(const PRM_Parm& parm) {
	uint32_t parmCount = parm.getMultiParmCount();
	if (!parm.isDefault())
		return false;

	for (int i = 0; i < parmCount; ++i) {
		PRM_Parm* parmInst = parm.getMultiParm(i);
		if (parmInst == nullptr || !parmInst->isDefault())
			return false;
	}
	return true;
}

void overrideMultiParmDefault(SOPAssign* node, const PRM_Parm& parm, const SOPAssign::CGAAttributeValueType& defArray,
                              fpreal time) {
	uint32_t defaultArraySize = getVariantArraySize(defArray);
	node->setFloat(parm.getToken(), 0, time, static_cast<double>(defaultArraySize));

	uint32_t parmCount = parm.getMultiParmCount();

	for (int i = 0; i < parmCount; ++i) {
		PRM_Parm* parmInst = parm.getMultiParm(i);
		if (parmInst == nullptr)
			continue;

		const PRM_Type& parmInstType = parmInst->getType();
		const std::string parmInstToken = parmInst->getToken();

		switch (parmInstType.getBasicType()) {
			case PRM_Type::PRM_BASIC_ORDINAL: {
				if (!std::holds_alternative<std::vector<bool>>(defArray))
					continue;

				const std::vector<bool>& boolValues = std::get<std::vector<bool>>(defArray);
				node->setInt(parmInstToken.c_str(), 0, time, boolValues[i]);
				break;
			}
			case PRM_Type::PRM_BASIC_FLOAT: {
				if (!std::holds_alternative<std::vector<double>>(defArray))
					continue;

				const std::vector<double>& doubleValues = std::get<std::vector<double>>(defArray);
				node->setFloat(parmInstToken.c_str(), 0, time, doubleValues[i]);
				break;
			}
			case PRM_Type::PRM_BASIC_STRING: {
				if (!std::holds_alternative<std::vector<std::wstring>>(defArray))
					continue;

				const std::vector<std::wstring>& wStringValues = std::get<std::vector<std::wstring>>(defArray);
				const UT_StringHolder stringValue(toOSNarrowFromUTF16(wStringValues[i]));
				node->setString(stringValue, CH_StringMeaning::CH_STRING_LITERAL, parmInstToken.c_str(), 0, time);
				break;
			}
			default:
				break;
		}
		parmInst->overwriteDefaults(time);
	}
}

void updateUIDefaultValues(SOPAssign* node, const std::wstring& style,
                           const SOPAssign::CGAAttributeValueMap& defaultValues) {
	const fpreal time = CHgetEvalTime();

	for (int parmIndex = 0; parmIndex < node->getNumParms(); ++parmIndex) {
		PRM_Parm& parm = node->getParm(parmIndex);

		if (parm.isSpareParm() && (parm.isDefault() || parm.isMultiParm())) {
			const UT_StringHolder attributeName(parm.getToken());
			const std::wstring ruleAttrName = NameConversion::toRuleAttr(style, attributeName);
			auto it = defaultValues.find(ruleAttrName);
			if (it == defaultValues.end())
				continue;

			const PRM_Type currParmType = parm.getType();

			switch (currParmType.getBasicType()) {
				case PRM_Type::PRM_BasicType::PRM_BASIC_ORDINAL: {
					switch (currParmType.getOrdinalType()) {
						case PRM_Type::PRM_ORD_NONE: {
							std::string stringValue;

							if (std::holds_alternative<std::wstring>(it->second))
								stringValue = toOSNarrowFromUTF16(std::get<std::wstring>(it->second));
							else if (std::holds_alternative<double>(it->second))
								stringValue = std::to_string(std::get<double>(it->second));
							else if (std::holds_alternative<bool>(it->second))
								stringValue = std::to_string(std::get<bool>(it->second));
							else
								continue;

							const PRM_ChoiceList* choices = parm.getTemplatePtr()->getChoiceListPtr();
							if (choices != nullptr) {
								const PRM_Name* choiceNames;
								choices->getChoiceNames(choiceNames);
								for (uint32_t choiceIdx = 0; choiceIdx < choices->getSize(&parm); ++choiceIdx) {
									const std::string currEnumString = choiceNames[choiceIdx].getToken();
									if (currEnumString == stringValue) {
										node->setInt(attributeName, 0, time, choiceIdx);
										break;
									}
								}
							}
							break;
						}
						case PRM_Type::PRM_ORD_TOGGLE: {
							if (!std::holds_alternative<bool>(it->second))
								continue;

							const int intValue = static_cast<int>(std::get<bool>(it->second));

							node->setInt(attributeName, 0, time, intValue);
							break;
						}
						default:
							break;
					}
					break;
				}
				case PRM_Type::PRM_BasicType::PRM_BASIC_FLOAT: {
					if (parm.getMultiType() == PRM_MultiType::PRM_MULTITYPE_LIST) {
						if (isMultiParmDefault(parm))
							overrideMultiParmDefault(node, parm, it->second, time);
						else
							continue; // avoid overriding if not default
					}
					else {
						switch (currParmType.getFloatType()) {
							case PRM_Type::PRM_FLOAT_RGBA: {
								if (!std::holds_alternative<std::wstring>(it->second))
									continue;

								const std::wstring colorString = std::get<std::wstring>(it->second);
								const auto color = AnnotationParsing::parseColor(colorString);

								node->setFloat(attributeName, 0, time, color[0]);
								node->setFloat(attributeName, 1, time, color[1]);
								node->setFloat(attributeName, 2, time, color[2]);
								break;
							}
							default: {
								if (!std::holds_alternative<double>(it->second))
									continue;

								const double floatValue = std::get<double>(it->second);

								node->setFloat(attributeName, 0, time, floatValue);
								break;
							}
						}
					}
					break;
				}
				case PRM_Type::PRM_BasicType::PRM_BASIC_STRING: {
					if (!std::holds_alternative<std::wstring>(it->second))
						continue;

					const UT_StringHolder stringValue(toOSNarrowFromUTF16(std::get<std::wstring>(it->second)));

					node->setString(stringValue, CH_StringMeaning::CH_STRING_LITERAL, attributeName, 0, time);
					break;
				}
				default: {
					// ignore all other types of parameters
					continue;
				}
			}
			parm.overwriteDefaults(time);
			if (!parm.isFactoryDefaultUI()) {
				PRM_Template* templatePtr = parm.getTemplatePtr();
				PRM_Default* factoryDefaults = templatePtr->getFactoryDefaults();

				for (size_t idx = 0; idx < templatePtr->getVectorSize(); ++idx) {
					PRM_Default* defaultValue = templatePtr->getDefault(idx);

					factoryDefaults[idx].set(defaultValue->getFloat(), defaultValue->getString(),
					                         defaultValue->getStringMeaning());
				}
			}
		}
	}
}

template <typename F>
bool visitMultiParmChildren(const PRM_Parm& parm, PRM_Type::PRM_BasicType expectedType, F fun) {
	const int parmCount = parm.getMultiParmCount();

	for (int i = 0; i < parmCount; ++i) {
		PRM_Parm* parmInst = parm.getMultiParm(i);
		if (parmInst == nullptr || parmInst->getType().getBasicType() != expectedType)
			return false; // invalid data

		fun(parmInst);
	}
	return true; // success
}

template <typename T, typename F>
std::optional<UT_ValArray<T>> getArrayFromMultiParm(const PRM_Parm& parm, PRM_Type::PRM_BasicType expectedType,
                                                    F eval) {
	const int parmCount = parm.getMultiParmCount();
	UT_ValArray<T> valArray(parmCount);
	bool success = visitMultiParmChildren(
	        parm, expectedType, [&valArray, eval](PRM_Parm* parmInst) { valArray.emplace_back(eval(parmInst)); });
	if (success)
		return valArray;
	else
		return {};
}

// identical to above function
template <typename T, typename F>
std::optional<std::vector<T>> getStdVectorFromMultiParm(const PRM_Parm& parm, PRM_Type::PRM_BasicType expectedType,
                                                        F eval) {
	const int parmCount = parm.getMultiParmCount();
	std::vector<T> vec;
	vec.reserve(parmCount);
	bool success = visitMultiParmChildren(parm, expectedType,
	                                      [&vec, eval](PRM_Parm* parmInst) { vec.emplace_back(eval(parmInst)); });
	if (success)
		return vec;
	else
		return {};
}

std::optional<UT_Int32Array> getBoolArrayFromParm(const SOPAssign* const node, const PRM_Parm& parm, fpreal time) {
	return getArrayFromMultiParm<int32>(parm, PRM_Type::PRM_BASIC_ORDINAL, [node, time](const PRM_Parm* parmInst) {
		return node->evalInt(parmInst, 0, time);
	});
}

std::optional<UT_FprealArray> getFloatArrayFromParm(const SOPAssign* const node, const PRM_Parm& parm, fpreal time) {
	return getArrayFromMultiParm<fpreal>(parm, PRM_Type::PRM_BASIC_FLOAT, [node, time](const PRM_Parm* parmInst) {
		return node->evalFloat(parmInst, 0, time);
	});
}

std::optional<UT_StringArray> getStringArrayFromParm(const SOPAssign* const node, const PRM_Parm& parm, fpreal time) {
	return getArrayFromMultiParm<UT_StringHolder>(parm, PRM_Type::PRM_BASIC_STRING,
	                                              [node, time](const PRM_Parm* parmInst) {
		                                              UT_StringHolder stringValue;
		                                              node->evalString(stringValue, parmInst, 0, time);
		                                              return stringValue;
	                                              });
}

std::optional<std::vector<bool>> getStdBoolVecFromParm(const SOPAssign* const node, const PRM_Parm& parm, fpreal time) {
	return getStdVectorFromMultiParm<bool>(parm, PRM_Type::PRM_BASIC_ORDINAL, [node, time](const PRM_Parm* parmInst) {
		return static_cast<bool>(node->evalInt(parmInst, 0, time));
	});
}

std::optional<std::vector<double>> getStdFloatVecFromParm(const SOPAssign* const node, const PRM_Parm& parm,
                                                          fpreal time) {
	return getStdVectorFromMultiParm<double>(parm, PRM_Type::PRM_BASIC_FLOAT, [node, time](const PRM_Parm* parmInst) {
		return static_cast<double>(node->evalFloat(parmInst, 0, time));
	});
}

std::optional<std::vector<std::wstring>> getStdStringVecFromParm(const SOPAssign* const node, const PRM_Parm& parm,
                                                                 fpreal time) {
	return getStdVectorFromMultiParm<std::wstring>(parm, PRM_Type::PRM_BASIC_STRING,
	                                               [node, time](const PRM_Parm* parmInst) {
		                                               UT_StringHolder stringValue;
		                                               node->evalString(stringValue, parmInst, 0, time);
		                                               return toUTF16FromOSNarrow(stringValue.c_str());
	                                               });
}

AttributeMapUPtr generateAttributeMapFromParameterValues(SOPAssign* node, const std::wstring& style) {
	const fpreal time = CHgetEvalTime();

	const int numParms = node->getNumParms();
	AttributeMapBuilderUPtr amb(prt::AttributeMapBuilder::create());

	for (int parmIndex = 0; parmIndex < numParms; ++parmIndex) {
		const PRM_Parm& parm = node->getParm(parmIndex);

		if (parm.isSpareParm() && (!parm.isDefault() || (parm.isMultiParm() && !isMultiParmDefault(parm)))) {
			const UT_StringHolder attributeName(parm.getToken());
			const std::wstring ruleAttrName = NameConversion::toRuleAttr(style, attributeName);

			const PRM_Type currParmType = parm.getType();

			switch (currParmType.getBasicType()) {
				case PRM_Type::PRM_BasicType::PRM_BASIC_ORDINAL: {
					switch (currParmType.getOrdinalType()) {
						case PRM_Type::PRM_ORD_NONE: {
							const int intValue = node->evalInt(&parm, 0, time);
							const PRM_ChoiceList* choices = parm.getTemplatePtr()->getChoiceListPtr();
							if (choices == nullptr)
								continue;
							UT_String result;
							if (!choices->tokenFromIndex(result, intValue))
								continue;

							auto it = node->mDefaultCGAAttributes.find(ruleAttrName);
							if (it == node->mDefaultCGAAttributes.end())
								continue;

							if (std::holds_alternative<std::wstring>(it->second)) {
								const std::wstring wstringValue = toUTF16FromOSNarrow(result.toStdString());
								amb->setString(ruleAttrName.c_str(), wstringValue.c_str());
							}
							else if (std::holds_alternative<double>(it->second)) {
								const double floatValue = result.toFloat();
								amb->setFloat(ruleAttrName.c_str(), floatValue);
							}
							else if (std::holds_alternative<bool>(it->second)) {
								const bool boolValue = static_cast<bool>(result.toInt());
								amb->setFloat(ruleAttrName.c_str(), boolValue);
							}
							break;
						}
						case PRM_Type::PRM_OrdinalType::PRM_ORD_TOGGLE: {
							const int intValue = node->evalInt(&parm, 0, time);
							amb->setBool(ruleAttrName.c_str(), static_cast<bool>(intValue));
							break;
						}
						default:
							break;
					}
					break;
				}
				case PRM_Type::PRM_BasicType::PRM_BASIC_FLOAT: {
					if (parm.getMultiType() == PRM_MultiType::PRM_MULTITYPE_LIST) {
						auto it = node->mDefaultCGAAttributes.find(ruleAttrName);
						if (it == node->mDefaultCGAAttributes.end())
							continue;

						if (std::holds_alternative<std::vector<std::wstring>>(it->second)) {
							const std::optional<std::vector<std::wstring>> wstringVec =
							        getStdStringVecFromParm(node, parm, time);
							if (!wstringVec.has_value())
								continue;

							std::vector<const wchar_t*> ptrWstringVec = toPtrVec(wstringVec.value());
							amb->setStringArray(ruleAttrName.c_str(), ptrWstringVec.data(), ptrWstringVec.size());
						}
						else if (std::holds_alternative<double>(it->second)) {
							const std::optional<std::vector<double>> doubleVec =
							        getStdFloatVecFromParm(node, parm, time);
							if (!doubleVec.has_value())
								continue;

							amb->setFloatArray(ruleAttrName.c_str(), doubleVec.value().data(),
							                   doubleVec.value().size());
						}
						else if (std::holds_alternative<bool>(it->second)) {
							const std::optional<std::vector<bool>> boolVec = getStdBoolVecFromParm(node, parm, time);
							if (!boolVec.has_value())
								continue;

							const std::vector<bool>& boolVecVal = boolVec.value();
							// convert compressed bool vec to native array
							auto boolArray = std::unique_ptr<bool[]>(new bool[boolVecVal.size()]);
							for (size_t i = 0; i < boolVecVal.size(); i++)
								boolArray[i] = boolVecVal[i];
							amb->setBoolArray(ruleAttrName.c_str(), boolArray.get(), boolVecVal.size());
						}
					}
					else {
						switch (currParmType.getFloatType()) {
							case PRM_Type::PRM_FLOAT_NONE: {
								const double floatValue = node->evalFloat(&parm, 0, time);

								amb->setFloat(ruleAttrName.c_str(), floatValue);
								break;
							}
							case PRM_Type::PRM_FLOAT_RGBA: {
								const float r = node->evalFloat(&parm, 0, time);
								const float g = node->evalFloat(&parm, 1, time);
								const float b = node->evalFloat(&parm, 2, time);
								const std::wstring colorString = AnnotationParsing::getColorString({r, g, b});

								amb->setString(ruleAttrName.c_str(), colorString.c_str());
								break;
							}
							default:
								break;
						}
					}
					break;
				}
				case PRM_Type::PRM_BasicType::PRM_BASIC_STRING: {
					UT_String stringValue;
					node->evalString(stringValue, &parm, 0, time);
					const std::wstring wstringValue = toUTF16FromOSNarrow(stringValue.toStdString());

					amb->setString(ruleAttrName.c_str(), wstringValue.c_str());
					break;
				}
				default: {
					// ignore all other types of parameters
					break;
				}
			}
		}
	}
	return AttributeMapUPtr(amb->createAttributeMap());
}

bool evaluateDefaultRuleAttributes(SOPAssign* node, const GU_Detail* detail, ShapeData& shapeData,
                                   const ShapeConverterUPtr& shapeConverter, const PRTContextUPtr& prtCtx,
                                   std::string& errors) {
	WA("all");

	assert(shapeData.isValid());

	// setup encoder options for attribute evaluation encoder
	constexpr const wchar_t* encs[] = {ENCODER_ID_CGA_EVALATTR};
	constexpr size_t encsCount = sizeof(encs) / sizeof(encs[0]);
	const AttributeMapUPtr encOpts = getValidEncoderInfo(ENCODER_ID_CGA_EVALATTR);
	const prt::AttributeMap* encsOpts[] = {encOpts.get()};

	const size_t numShapes = shapeData.getInitialShapeBuilders().size();

	// keep rule file info per initial shape to filter generated attributes
	std::vector<RuleFileInfoUPtr> ruleFileInfos(numShapes);

	// create initial shapes
	// loop over all initial shapes and use the first primitive to get the attribute values
	for (size_t isIdx = 0; isIdx < numShapes; isIdx++) {
		const auto& pv = shapeData.getPrimitiveMapping(isIdx);
		const auto& firstPrimitive = pv.front();

		const MainAttributes& ma = shapeConverter->getMainAttributesFromPrimitive(detail, firstPrimitive);

		// try to get a resolve map
		ResolveMapSPtr resolveMap = prtCtx->getResolveMap(ma.mRPK);
		if (!resolveMap) {
			errors.append("Could not read Rule Package '")
			        .append(ma.mRPK.string())
			        .append("', aborting default rule attribute evaluation");
			LOG_ERR << errors;
			return false;
		}

		ruleFileInfos[isIdx] = getRuleFileInfo(ma, resolveMap, prtCtx->mPRTCache.get());

		const std::wstring shapeName = L"shape_" + std::to_wstring(isIdx);
		if (DBG)
			LOG_DBG << "evaluating attrs for shape: " << shapeName;

		// persist rule attributes even if empty (need to live until prt::generate is done)
		AttributeMapBuilderUPtr amb(prt::AttributeMapBuilder::create());
		AttributeMapUPtr ruleAttr = generateAttributeMapFromParameterValues(node, node->getStyle());

		auto& isb = shapeData.getInitialShapeBuilder(isIdx);

		int32_t randomSeed;
		if (ma.mOverrideSeed)
			randomSeed = ma.mSeed;
		else
			randomSeed = shapeData.getInitialShapeRandomSeed(isIdx);

		std::vector<std::pair<std::wstring, std::wstring>> cgbs; // key -> uri
		getCGBs(resolveMap, cgbs);
		if (cgbs.empty()) {
			LOG_ERR << "no rule files found in rule package";
			return false;
		}
		const std::wstring ruleFile = cgbs.front().first;

		isb->setAttributes(ruleFile.c_str(), ma.mStartRule.c_str(), randomSeed, shapeName.c_str(), ruleAttr.get(),
		                   resolveMap.get());

		prt::Status status = prt::STATUS_UNSPECIFIED_ERROR;
		const prt::InitialShape* initialShape = isb->createInitialShapeAndReset(&status);
		if (status == prt::STATUS_OK && initialShape != nullptr) {
			shapeData.addShape(initialShape, std::move(amb), std::move(ruleAttr));
		}
		else
			LOG_WRN << "failed to create initial shape " << shapeName << ": " << prt::getStatusDescription(status);
	}
	assert(shapeData.isValid());

	// run generate to evaluate default rule attributes
	AttrEvalCallbacks aec(shapeData.getRuleAttributeMapBuilders(), ruleFileInfos);
	const InitialShapeNOPtrVector& is = shapeData.getInitialShapes();
	const prt::Status stat = prt::generate(is.data(), is.size(), nullptr, encs, encsCount, encsOpts, &aec,
	                                       prtCtx->mPRTCache.get(), nullptr, nullptr, nullptr);
	if (stat != prt::STATUS_OK) {
		errors.append("Failed to evaluate default attributes with status: '")
		        .append(prt::getStatusDescription(stat))
		        .append("' (")
		        .append(std::to_string(stat))
		        .append(")");
		LOG_ERR << errors;
		return false;
	}

	assert(shapeData.isValid());

	return true;
}

SOPAssign::CGAAttributeValueType getDefaultCGAAttrValue(const SOPAssign::CGAAttributeValueMap& cgaAttrMap, const std::wstring& key) {
	const auto& defaultValIt = cgaAttrMap.find(key);
	if (defaultValIt != cgaAttrMap.end())
		return defaultValIt->second;
	return {};
}

bool getDefaultBool(const SOPAssign::CGAAttributeValueType& defaultValue) {
	if (std::holds_alternative<bool>(defaultValue))
		return std::get<bool>(defaultValue);
	return false;
}

double getDefaultFloat(const SOPAssign::CGAAttributeValueType& defaultValue) {
	if (std::holds_alternative<double>(defaultValue))
		return std::get<double>(defaultValue);
	return 0.0;
}

std::wstring getDefaultString(const SOPAssign::CGAAttributeValueType& defaultValue) {
	if (std::holds_alternative<std::wstring>(defaultValue))
		return std::get<std::wstring>(defaultValue);
	return {};
}

std::vector<bool> getDefaultBoolVec(const SOPAssign::CGAAttributeValueType& defaultValue) {
	if (std::holds_alternative<std::vector<double>>(defaultValue))
		return std::get<std::vector<bool>>(defaultValue);
	return {};
}

std::vector<double> getDefaultFloatVec(const SOPAssign::CGAAttributeValueType& defaultValue) {
	if (std::holds_alternative<std::vector<double>>(defaultValue))
		return std::get<std::vector<double>>(defaultValue);
	return {};
}

std::vector<std::wstring> getDefaultStringVec(const SOPAssign::CGAAttributeValueType& defaultValue) {
	if (std::holds_alternative<std::vector<std::wstring>>(defaultValue))
		return std::get<std::vector<std::wstring>>(defaultValue);
	return {};
}



std::wstring getDescription(const AnnotationParsing::TraitParameterMap& traitParmMap) {
	const auto& descriptionIt = traitParmMap.find(AnnotationParsing::AttributeTrait::DESCRIPTION);
	if (descriptionIt != traitParmMap.end() && std::holds_alternative<std::wstring>(descriptionIt->second))
		return std::get<std::wstring>(descriptionIt->second);

	return {};
}

AnnotationParsing::RangeAnnotation getRange(const AnnotationParsing::TraitParameterMap& traitParmMap,
                                            std::pair<double, double> minMax, bool restricted) {
	const auto& rangeIt = traitParmMap.find(AnnotationParsing::AttributeTrait::RANGE);
	if (rangeIt != traitParmMap.end())
		return std::get<AnnotationParsing::RangeAnnotation>(rangeIt->second);

	return AnnotationParsing::RangeAnnotation({minMax, restricted});
};

bool tryHandleEnum(SOPAssign* node, const std::wstring attrId, const std::wstring attrName, std::wstring defaultValue,
                   const AnnotationParsing::TraitParameterMap& traitParmMap, const std::wstring& description,
                   std::vector<std::wstring> parentFolders) {
	const auto& enumIt = traitParmMap.find(AnnotationParsing::AttributeTrait::ENUM);
	if (enumIt == traitParmMap.end())
		return false;

	AnnotationParsing::EnumAnnotation enumAnnotation = std::get<AnnotationParsing::EnumAnnotation>(enumIt->second);

	NodeSpareParameter::addEnumParm(node, attrId, attrName, defaultValue, enumAnnotation.mOptions, parentFolders,
	                                description);
	return true;
}

bool tryHandleRange(SOPAssign* node, const std::wstring attrId, const std::wstring attrName, double defaultValue,
                    const AnnotationParsing::TraitParameterMap& traitParmMap, const std::wstring& description,
                    std::vector<std::wstring> parentFolders) {
	const bool isAngle = (traitParmMap.find(AnnotationParsing::AttributeTrait::ANGLE) != traitParmMap.end());
	const bool isPercent = (traitParmMap.find(AnnotationParsing::AttributeTrait::PERCENT) != traitParmMap.end());
	const bool isRange = (traitParmMap.find(AnnotationParsing::AttributeTrait::RANGE) != traitParmMap.end());
	if (!isAngle && !isPercent && !isRange)
		return false;

	double scalingFactor = 1.0;
	std::pair<double, double> minMax = std::make_pair(0.0, 10.0);

	if (isPercent) {
		minMax = std::make_pair(0.0, 1.0);
		scalingFactor = 100.0;
	}
	else if (isAngle) {
		minMax = std::make_pair(0.0, 360.0);
	}

	AnnotationParsing::RangeAnnotation rangeAnnotation = getRange(traitParmMap, minMax, false);

	NodeSpareParameter::addFloatParm(node, attrId, attrName, defaultValue, scalingFactor * rangeAnnotation.minMax.first,
	                                 scalingFactor * rangeAnnotation.minMax.second, rangeAnnotation.restricted,
	                                 isPercent, parentFolders, description);
	return true;
}

bool tryHandleFile(SOPAssign* node, const std::wstring attrId, const std::wstring attrName, std::wstring defaultValue,
                   const AnnotationParsing::TraitParameterMap& traitParmMap, const std::wstring& description,
                   std::vector<std::wstring> parentFolders) {
	const auto& fileIt = traitParmMap.find(AnnotationParsing::AttributeTrait::FILE);
	if (fileIt == traitParmMap.end())
		return false;

	AnnotationParsing::FileAnnotation fileAnnotation = std::get<AnnotationParsing::FileAnnotation>(fileIt->second);

	NodeSpareParameter::addFileParm(node, attrId, attrName, defaultValue, fileAnnotation, parentFolders, description);
	return true;
}

bool tryHandleDir(SOPAssign* node, const std::wstring attrId, const std::wstring attrName, std::wstring defaultValue,
                  const AnnotationParsing::TraitParameterMap& traitParmMap, const std::wstring& description,
                  std::vector<std::wstring> parentFolders) {
	const auto& dirIt = traitParmMap.find(AnnotationParsing::AttributeTrait::DIR);
	if (dirIt == traitParmMap.end())
		return false;

	NodeSpareParameter::addDirectoryParm(node, attrId, attrName, defaultValue, parentFolders, description);
	return true;
}

bool tryHandleColor(SOPAssign* node, const std::wstring attrId, const std::wstring attrName, std::wstring defaultValue,
                    const AnnotationParsing::TraitParameterMap& traitParmMap, const std::wstring& description,
                    std::vector<std::wstring> parentFolders) {
	const auto& colorIt = traitParmMap.find(AnnotationParsing::AttributeTrait::COLOR);
	if (colorIt == traitParmMap.end())
		return false;

	const AnnotationParsing::ColorAnnotation& color = AnnotationParsing::parseColor(defaultValue);
	NodeSpareParameter::addColorParm(node, attrId, attrName, color, parentFolders, description);
	return true;
}

} // namespace

SOPAssign::SOPAssign(const PRTContextUPtr& pCtx, OP_Network* net, const char* name, OP_Operator* op)
    : SOP_Node(net, name, op), mPRTCtx(pCtx), mShapeConverter(new ShapeConverter()) {}

void SOPAssign::updateDefaultCGAAttributes(const ShapeData& shapeData) {
	mDefaultCGAAttributes.clear();

	AttributeMapVector defaultRuleAttributeMaps;
	for (auto& amb : shapeData.getRuleAttributeMapBuilders()) {
		defaultRuleAttributeMaps.emplace_back(amb->createAttributeMap());
	}

	for (size_t isIdx = 0; isIdx < defaultRuleAttributeMaps.size(); isIdx++) {
		const auto& defaultRuleAttributes = defaultRuleAttributeMaps[isIdx];

		size_t keyCount = 0;
		const wchar_t* const* cKeys = defaultRuleAttributes->getKeys(&keyCount);
		for (size_t k = 0; k < keyCount; k++) {
			const wchar_t* const key = cKeys[k];
			const auto type = defaultRuleAttributes->getType(key);

			CGAAttributeValueType defVal;
			switch (type) {
				case prt::AttributeMap::PT_FLOAT: {
					defVal = defaultRuleAttributes->getFloat(key);
					break;
				}
				case prt::AttributeMap::PT_BOOL: {
					defVal = defaultRuleAttributes->getBool(key);
					break;
				}
				case prt::AttributeMap::PT_STRING: {
					const wchar_t* v = defaultRuleAttributes->getString(key);
					assert(v != nullptr);
					defVal = std::wstring(v);
					break;
				}
				case prt::AttributeMap::PT_FLOAT_ARRAY: {
					size_t arr_length = 0;
					const double* doubleArray = defaultRuleAttributes->getFloatArray(key, &arr_length);
					std::vector<double> doubleVec;
					doubleVec.reserve(arr_length);

					for (short enumIndex = 0; enumIndex < arr_length; enumIndex++)
						doubleVec.emplace_back(doubleArray[enumIndex]);

					defVal = doubleVec;
					break;
				}
				case prt::AttributeMap::PT_BOOL_ARRAY: {
					size_t arr_length = 0;
					const bool* boolArray = defaultRuleAttributes->getBoolArray(key, &arr_length);
					std::vector<bool> boolVec;
					boolVec.reserve(arr_length);

					for (short enumIndex = 0; enumIndex < arr_length; enumIndex++)
						boolVec.emplace_back(boolArray[enumIndex]);

					defVal = boolVec;
					break;
				}
				case prt::AttributeMap::PT_STRING_ARRAY: {
					size_t arr_length = 0;
					const wchar_t* const* stringArray = defaultRuleAttributes->getStringArray(key, &arr_length);
					assert((stringArray != nullptr) || (arr_length == 0));

					std::vector<std::wstring> stringVec;
					stringVec.reserve(arr_length);

					for (short enumIndex = 0; enumIndex < arr_length; enumIndex++)
						stringVec.emplace_back(stringArray[enumIndex]);

					defVal = stringVec;
					break;
				}
				default:
					continue;
			}
			mDefaultCGAAttributes.emplace(key, defVal);
		}
	}
}

void SOPAssign::updatePrimitiveAttributes(GU_Detail* detail) {
	const fpreal time = CHgetEvalTime();

	const int numParms = getNumParms();

	for (int parmIndex = 0; parmIndex < numParms; ++parmIndex) {
		const PRM_Parm& parm = getParm(parmIndex);

		if (parm.isSpareParm() && (!parm.isDefault() || (parm.isMultiParm() && !isMultiParmDefault(parm)))) {
			const UT_StringHolder attributeName(parm.getToken());
			const std::wstring style = getStyle();
			const std::wstring ruleAttrName = NameConversion::toRuleAttr(style, attributeName);
			auto it = mDefaultCGAAttributes.find(ruleAttrName);
			if (it == mDefaultCGAAttributes.end())
				continue;
			const PRM_Type currParmType = parm.getType();
			GA_AttributeOwner attrOwner = getGroupAttribOwner(GA_GroupType::GA_GROUP_PRIMITIVE);

			switch (currParmType.getBasicType()) {
				case PRM_Type::PRM_BasicType::PRM_BASIC_ORDINAL: {
					switch (currParmType.getOrdinalType()) {
						case PRM_Type::PRM_ORD_NONE: {
							const int intValue = evalInt(&parm, 0, time);
							const PRM_ChoiceList* choices = parm.getTemplatePtr()->getChoiceListPtr();
							if (choices != nullptr) {
								UT_String result;
								if (choices->tokenFromIndex(result, intValue)) {
									GA_RWHandleS stringHandle(detail->addStringTuple(attrOwner, attributeName, 1));
									stringHandle.set(0, result);
								}
							}
							break;
						}
						case PRM_Type::PRM_ORD_TOGGLE: {
							const int intValue = evalInt(&parm, 0, time);
							GA_RWHandleI ordinalHandle(detail->addIntTuple(attrOwner, attributeName, 1, GA_Defaults(0),
							                                               nullptr, nullptr, GA_STORE_INT8));
							ordinalHandle.set(0, intValue);
							break;
						}
						default:
							break;
					}
					break;
				}
				case PRM_Type::PRM_BasicType::PRM_BASIC_FLOAT: {
					if (parm.getMultiType() == PRM_MultiType::PRM_MULTITYPE_LIST) {
						if (std::holds_alternative<std::vector<bool>>(it->second)) {
							const std::optional<UT_Int32Array> boolArray = getBoolArrayFromParm(this, parm, time);

							if (!boolArray.has_value())
								break;

							GA_RWHandleT<UT_Int32Array> intArrayHandle(
							        detail->addIntArray(attrOwner, attributeName, 1, nullptr, nullptr, GA_STORE_INT8));
							intArrayHandle.set(0, boolArray.value());
						}
						else if (std::holds_alternative<std::vector<double>>(it->second)) {
							const std::optional<UT_FprealArray> floatArray = getFloatArrayFromParm(this, parm, time);

							if (!floatArray.has_value())
								break;

							GA_RWHandleDA floatArrayHandle(detail->addFloatArray(attrOwner, attributeName, 1));
							floatArrayHandle.set(0, floatArray.value());
						}
						else if (std::holds_alternative<std::vector<std::wstring>>(it->second)) {
							const std::optional<UT_StringArray> stringArray = getStringArrayFromParm(this, parm, time);

							if (!stringArray.has_value())
								break;

							GA_RWHandleSA stringArrayHandle(detail->addStringArray(attrOwner, attributeName, 1));
							stringArrayHandle.set(0, stringArray.value());
						}
					}
					else {
						switch (currParmType.getFloatType()) {
							case PRM_Type::PRM_FLOAT_NONE: {
								double floatValue = evalFloat(&parm, 0, time);

								const PRM_SpareData* spareData = parm.getSparePtr();
								if (spareData != nullptr) {
									const char* isPercent =
										spareData->getValue(NodeSpareParameter::PRM_SPARE_IS_PERCENT_TOKEN);
									if ((isPercent != nullptr) && (strcmp(isPercent, "true") == 0))
										floatValue /= PERCENT_FACTOR;
								}

								GA_RWHandleD floatHandle(detail->addFloatTuple(attrOwner, attributeName, 1));
								floatHandle.set(0, floatValue);
								break;
							}
							case PRM_Type::PRM_FLOAT_RGBA: {
								const float r = evalFloat(&parm, 0, time);
								const float g = evalFloat(&parm, 1, time);
								const float b = evalFloat(&parm, 2, time);
								const std::wstring colorString = AnnotationParsing::getColorString({r, g, b});

								UT_String stringValue(toOSNarrowFromUTF16(colorString));
								GA_RWHandleS stringHandle(detail->addStringTuple(attrOwner, attributeName, 1));
								stringHandle.set(0, stringValue);
								break;
							}
							default:
								break;
						}
					}
					break;
				}
				case PRM_Type::PRM_BasicType::PRM_BASIC_STRING: {
					UT_String stringValue;
					evalString(stringValue, &parm, 0, time);

					GA_RWHandleS stringHandle(detail->addStringTuple(attrOwner, attributeName, 1));
					stringHandle.set(0, stringValue);
					break;
				}
				default: {
					// ignore all other types of parameters
					break;
				}
			}
		}
	}
};

void SOPAssign::buildUI(GU_Detail* detail, ShapeData& shapeData, const ShapeConverterUPtr& shapeConverter,
                        const PRTContextUPtr& prtCtx, std::string& errors) {
	const auto& pv = shapeData.getPrimitiveMapping(0);
	const auto& firstPrimitive = pv.front();
	const MainAttributes& ma = shapeConverter->getMainAttributesFromPrimitive(detail, firstPrimitive);

	// try to get a resolve map
	const ResolveMapSPtr& resolveMap = prtCtx->getResolveMap(ma.mRPK);
	if (!resolveMap) {
		errors.append("Could not read Rule Package '")
		        .append(ma.mRPK.string())
		        .append("', aborting default rule attribute evaluation");
		LOG_ERR << errors;
		return;
	}

	const RuleFileInfoUPtr& ruleFileInfo = getRuleFileInfo(ma, resolveMap, prtCtx->mPRTCache.get());
	const RuleAttributeSet& ruleAttributes =
	        ruleFileInfo ? getRuleAttributes(ma.mRPK.generic_wstring(), ruleFileInfo.get()) : RuleAttributeSet();

	const AnnotationParsing::AttributeTraitMap& attributeAnnotations =
	        ruleFileInfo ? AnnotationParsing::getAttributeAnnotations(ruleFileInfo)
	                     : AnnotationParsing::AttributeTraitMap();

	NodeSpareParameter::clearAllParms(this);
	NodeSpareParameter::addSeparator(this);
	NodeSpareParameter::addSimpleFolder(this, RULE_ATTRIBUTES_FOLDER_NAME);

	for (const auto& ra : ruleAttributes) {
		const std::wstring attrName = ra.niceName;
		const std::wstring attrId = toUTF16FromOSNarrow(NameConversion::toPrimAttr(ra.fqName).toStdString());

		const auto& annotationsIt = attributeAnnotations.find(ra.fqName);
		if (annotationsIt == attributeAnnotations.end())
			continue;
		const AnnotationParsing::TraitParameterMap& traitParmMap = annotationsIt->second;

		const std::wstring& description = getDescription(traitParmMap);

		FolderVec parentFolders;
		parentFolders.push_back(RULE_ATTRIBUTES_FOLDER_NAME);
		parentFolders.push_back(ra.ruleFile);
		parentFolders.insert(parentFolders.end(), ra.groups.begin(), ra.groups.end());

		const CGAAttributeValueType& defaultCGAAttrValue = getDefaultCGAAttrValue(mDefaultCGAAttributes, ra.fqName);

		switch (ra.mType) {
			case prt::AnnotationArgumentType::AAT_BOOL: {
				const bool defaultValue = getDefaultBool(defaultCGAAttrValue);

				if (tryHandleEnum(this, attrId, attrName, std::to_wstring(defaultValue), traitParmMap, description,
				                  parentFolders)) {
					continue;
				}

				NodeSpareParameter::addBoolParm(this, attrId, attrName, defaultValue, parentFolders, description);
				break;
			}
			case prt::AnnotationArgumentType::AAT_FLOAT: {
				const double defaultValue = getDefaultFloat(defaultCGAAttrValue);

				if (tryHandleEnum(this, attrId, attrName, std::to_wstring(defaultValue), traitParmMap, description,
				                  parentFolders)) {
					continue;
				}

				if (tryHandleRange(this, attrId, attrName, defaultValue, traitParmMap, description, parentFolders)) {
					continue;
				}

				NodeSpareParameter::addFloatParm(this, attrId, attrName, defaultValue, 0.0, 10.0, true, false,
				                                 parentFolders, description);
				break;
			}
			case prt::AnnotationArgumentType::AAT_STR: {
				const std::wstring defaultValue = getDefaultString(defaultCGAAttrValue);

				if (tryHandleEnum(this, attrId, attrName, defaultValue, traitParmMap, description, parentFolders)) {
					continue;
				}

				if (tryHandleFile(this, attrId, attrName, defaultValue, traitParmMap, description, parentFolders)) {
					continue;
				}

				if (tryHandleDir(this, attrId, attrName, defaultValue, traitParmMap, description, parentFolders)) {
					continue;
				}

				if (tryHandleColor(this, attrId, attrName, defaultValue, traitParmMap, description, parentFolders)) {
					continue;
				}

				NodeSpareParameter::addStringParm(this, attrId, attrName, defaultValue, parentFolders, description);
				break;
			}
			case prt::AnnotationArgumentType::AAT_BOOL_ARRAY: {
				const std::vector<bool> defaultValues = getDefaultBoolVec(defaultCGAAttrValue);
				NodeSpareParameter::addBoolArrayParm(this, attrId, attrName, defaultValues, parentFolders, description);
				break;
			}
			case prt::AnnotationArgumentType::AAT_FLOAT_ARRAY: {
				const std::vector<double> defaultValues = getDefaultFloatVec(defaultCGAAttrValue);
				NodeSpareParameter::addFloatArrayParm(this, attrId, attrName, defaultValues, parentFolders,
					description);
				break;
			}
			case prt::AnnotationArgumentType::AAT_STR_ARRAY: {
				const std::vector<std::wstring> defaultValues = getDefaultStringVec(defaultCGAAttrValue);
				NodeSpareParameter::addStringArrayParm(this, attrId, attrName, defaultValues, parentFolders,
					description);
				break;
			}
			default:
				break;
		}
	}
}

OP_ERROR SOPAssign::cookMySop(OP_Context& context) {
	WA_NEW_LAP
	WA("all");

	logging::ScopedLogLevelModifier scopedLogLevel(CommonNodeParams::getLogLevel(this, context.getTime()));

	if (lockInputs(context) >= UT_ERROR_ABORT) {
		LOG_DBG << "lockInputs error";
		return error();
	}
	duplicateSource(0, context);

	if (error() < UT_ERROR_ABORT && cookInputGroups(context) < UT_ERROR_ABORT) {
		UT_AutoInterrupt progress("Assigning CityEngine rule attributes...");

		PrimitiveClassifier primCls(this, gdp, context);
		mShapeConverter->getMainAttributes(this, context);

		ShapeData shapeData;
		mShapeConverter->get(gdp, primCls, shapeData, mPRTCtx);
		std::string evalAttrErrorMessage;
		const bool canContinue =
		        evaluateDefaultRuleAttributes(this, gdp, shapeData, mShapeConverter, mPRTCtx, evalAttrErrorMessage);
		if (!canContinue) {
			const std::string errMsg =
			        "Could not successfully evaluate default rule attributes:\n" + evalAttrErrorMessage;
			LOG_ERR << getName() << ": " << errMsg;
			addError(SOP_MESSAGE, errMsg.c_str());
			return UT_ERROR_ABORT;
		}
		auto oldAttributes = mDefaultCGAAttributes;
		updateDefaultCGAAttributes(shapeData);
		if (!compareAttributeTypes(oldAttributes, mDefaultCGAAttributes) && !mWasJustLoaded)
			buildUI(gdp, shapeData, mShapeConverter, mPRTCtx, evalAttrErrorMessage);
		updateUIDefaultValues(this, getStyle(), mDefaultCGAAttributes);
		updatePrimitiveAttributes(gdp);

		mShapeConverter->put(gdp, primCls, shapeData);
	}

	unlockInputs();

	mWasJustLoaded = false;
	return error();
}

bool SOPAssign::load(UT_IStream& is, const char* extension, const char* path) {
	SOP_Node::load(is, extension, path);
	mWasJustLoaded = true;
	return false;
}

void SOPAssign::opChanged(OP_EventType reason, void* data) {
	SOP_Node::opChanged(reason, data);

	// trigger recook on name change, we use the node name in various output places
	if (reason == OP_NAME_CHANGED)
		forceRecook();

	// trigger recook on parameter/attribute change
	if (reason == OP_PARM_CHANGED) {
		// parm index is directly stored in the void*, casting to size_t to avoid compiler warnings
		const PRM_ParmList* parmList = getParmList();
		if (parmList == nullptr)
			return;

		const std::uintptr_t parmIdx = reinterpret_cast<std::uintptr_t>(data);
		const PRM_Parm* const parmPtr = parmList->getParmPtr(parmIdx);
		if (parmPtr == nullptr)
			return;

		if (parmPtr->isSpareParm()) {
			const PRM_Type currParmType = parmPtr->getType();

			const bool isOrdParm = (currParmType.getBasicType() == PRM_Type::PRM_BasicType::PRM_BASIC_ORDINAL) &&
			                       ((currParmType.getOrdinalType() == PRM_Type::PRM_OrdinalType::PRM_ORD_TOGGLE) ||
			                        (currParmType.getOrdinalType() == PRM_Type::PRM_OrdinalType::PRM_ORD_NONE));
			const bool isFloatParm = (currParmType.getBasicType() == PRM_Type::PRM_BasicType::PRM_BASIC_FLOAT);
			const bool isStringParm = (currParmType.getBasicType() == PRM_Type::PRM_BasicType::PRM_BASIC_STRING);

			if (isOrdParm || isFloatParm || isStringParm)
				forceRecook();
		}
	}
}
