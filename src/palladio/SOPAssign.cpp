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
	if (!resolveMap->hasKey(ma.mRuleFile.c_str())) // workaround for bug in getString
		return {};

	const auto cgbURI = resolveMap->getString(ma.mRuleFile.c_str());
	if (cgbURI == nullptr)
		return {};

	prt::Status status = prt::STATUS_UNSPECIFIED_ERROR;
	RuleFileInfoUPtr rfi(prt::createRuleFileInfo(cgbURI, prtCache, &status));
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

void updateUIDefaultValues(SOPAssign* node, const std::wstring& style,
                           const SOPAssign::CGAAttributeValueMap& defaultValues) {
	const fpreal time = CHgetEvalTime();

	const int numParms = node->getNumParms();

	for (int parmIndex = 0; parmIndex < numParms; ++parmIndex) {
		PRM_Parm& parm = node->getParm(parmIndex);

		if (parm.isSpareParm() && parm.isDefault()) {
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

							switch (it->second.index()) {
								case 0: {
									stringValue = toOSNarrowFromUTF16(std::get<std::wstring>(it->second));
									break;
								}
								case 1: {
									stringValue = std::to_string(std::get<double>(it->second));
									break;
								}
								case 2: {
									stringValue = std::to_string(std::get<bool>(it->second));
									break;
								}
								default:
									continue;
							}

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
							if (it->second.index() != 2)
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
					switch (currParmType.getFloatType()) {
						case PRM_Type::PRM_FLOAT_RGBA: {
							if (it->second.index() != 0)
								continue;

							const std::wstring colorString = std::get<std::wstring>(it->second);
							const auto color = AnnotationParsing::parseColor(colorString);

							node->setFloat(attributeName, 0, time, color[0]);
							node->setFloat(attributeName, 1, time, color[1]);
							node->setFloat(attributeName, 2, time, color[2]);
							break;
						}
						default: {
							if (it->second.index() != 1)
								continue;

							const double floatValue = std::get<double>(it->second);

							node->setFloat(attributeName, 0, time, floatValue);
							break;
						}
					}
					break;
				}
				case PRM_Type::PRM_BasicType::PRM_BASIC_STRING: {
					if (it->second.index() != 0)
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

AttributeMapUPtr generateAttributeMapFromParameterValues(SOPAssign* node, const std::wstring& style) {
	const fpreal time = CHgetEvalTime();

	const int numParms = node->getNumParms();
	AttributeMapBuilderUPtr amb(prt::AttributeMapBuilder::create());

	for (int parmIndex = 0; parmIndex < numParms; ++parmIndex) {
		const PRM_Parm& parm = node->getParm(parmIndex);

		if (parm.isSpareParm() && !parm.isDefault()) {
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

							switch (it->second.index()) {
								case 0: {
									const std::wstring wstringValue = toUTF16FromOSNarrow(result.toStdString());
									amb->setString(ruleAttrName.c_str(), wstringValue.c_str());
									break;
								}
								case 1: {
									const double floatValue = result.toFloat();
									amb->setFloat(ruleAttrName.c_str(), floatValue);
									break;
								}
								case 2: {
									const bool boolValue = static_cast<bool>(result.toInt());
									amb->setFloat(ruleAttrName.c_str(), boolValue);
									break;
								}
								default:
									break;
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

		isb->setAttributes(ma.mRuleFile.c_str(), ma.mStartRule.c_str(), randomSeed, shapeName.c_str(), ruleAttr.get(),
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
					assert(defVal.index() == 0); // std::wstring is type index 0
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

		if (parm.isSpareParm() && !parm.isDefault()) {
			const UT_StringHolder attributeName(parm.getToken());
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
	const RuleAttributeSet& ruleAttributes = getRuleAttributes(ma.mRPK.generic_wstring(), ruleFileInfo.get());

	NodeSpareParameter::clearAllParms(this);
	NodeSpareParameter::addSeparator(this);
	NodeSpareParameter::addSimpleFolder(this, RULE_ATTRIBUTES_FOLDER_NAME);

	for (const auto& ra : ruleAttributes) {
		const std::wstring attrName = ra.niceName;
		const std::wstring attrId = toUTF16FromOSNarrow(NameConversion::toPrimAttr(ra.fqName).toStdString());

		const auto annotationInfo = AnnotationParsing::getAttributeAnnotationInfo(ra.fqName, ruleFileInfo);
		const std::wstring description = annotationInfo.mDescription;

		FolderVec parentFolders;
		parentFolders.push_back(RULE_ATTRIBUTES_FOLDER_NAME);
		parentFolders.push_back(ra.ruleFile);
		parentFolders.insert(parentFolders.end(), ra.groups.begin(), ra.groups.end());

		const auto defaultValIt = mDefaultCGAAttributes.find(ra.fqName);
		const bool foundDefaultValue = (defaultValIt != mDefaultCGAAttributes.end());

		switch (ra.mType) {
			case prt::AnnotationArgumentType::AAT_BOOL: {
				const bool isDefaultValBool = (defaultValIt->second.index() == 2);
				const bool defaultValue =
				        (foundDefaultValue && isDefaultValBool) ? std::get<bool>(defaultValIt->second) : false;

				switch (annotationInfo.mAttributeTrait) {
					case AnnotationParsing::AttributeTrait::ENUM: {
						AnnotationParsing::EnumAnnotation enumAnnotation =
						        AnnotationParsing::parseEnumAnnotation(annotationInfo.mAnnotation);

						NodeSpareParameter::addEnumParm(this, attrId, attrName, std::to_wstring(defaultValue),
						                                enumAnnotation.mOptions, parentFolders, description);
						break;
					}
					default: {
						NodeSpareParameter::addBoolParm(this, attrId, attrName, defaultValue, parentFolders,
						                                description);
						break;
					}
				}
				break;
			}
			case prt::AnnotationArgumentType::AAT_FLOAT: {
				const bool isDefaultValFloat = (defaultValIt->second.index() == 1);
				const double defaultValue =
				        (foundDefaultValue && isDefaultValFloat) ? std::get<double>(defaultValIt->second) : 0.0;

				switch (annotationInfo.mAttributeTrait) {
					case AnnotationParsing::AttributeTrait::ENUM: {
						AnnotationParsing::EnumAnnotation enumAnnotation =
						        AnnotationParsing::parseEnumAnnotation(annotationInfo.mAnnotation);

						NodeSpareParameter::addEnumParm(this, attrId, attrName, std::to_wstring(defaultValue),
						                                enumAnnotation.mOptions, parentFolders, description);
						break;
					}
					case AnnotationParsing::AttributeTrait::RANGE: {
						AnnotationParsing::RangeAnnotation rangeAnnotation =
						        AnnotationParsing::parseRangeAnnotation(annotationInfo.mAnnotation);

						NodeSpareParameter::addFloatParm(this, attrId, attrName, defaultValue, rangeAnnotation.min,
						                                 rangeAnnotation.max, rangeAnnotation.restricted, false,
						                                 parentFolders, description);
						break;
					}
					case AnnotationParsing::AttributeTrait::ANGLE: {
						AnnotationParsing::RangeAnnotation rangeAnnotation =
						        AnnotationParsing::parseRangeAnnotation(annotationInfo.mAnnotation, 0, 360);
						NodeSpareParameter::addFloatParm(this, attrId, attrName, defaultValue, rangeAnnotation.min,
						                                 rangeAnnotation.max, false, false, parentFolders, description);
						break;
					}
					case AnnotationParsing::AttributeTrait::PERCENT: {
						AnnotationParsing::RangeAnnotation rangeAnnotation =
						        AnnotationParsing::parseRangeAnnotation(annotationInfo.mAnnotation, 0, 1);
						NodeSpareParameter::addFloatParm(this, attrId, attrName, defaultValue * PERCENT_FACTOR,
						                                 rangeAnnotation.min * PERCENT_FACTOR,
						                                 rangeAnnotation.max * PERCENT_FACTOR, false, true,
						                                 parentFolders, description);
						break;
					}
					default: {
						NodeSpareParameter::addFloatParm(this, attrId, attrName, defaultValue, 0, 10, true, false,
						                                 parentFolders, description);
						break;
					}
				}

				break;
			}
			case prt::AnnotationArgumentType::AAT_STR: {
				const bool isDefaultValString = (defaultValIt->second.index() == 0);
				const std::wstring defaultValue =
				        (foundDefaultValue && isDefaultValString) ? std::get<std::wstring>(defaultValIt->second) : L"";

				switch (annotationInfo.mAttributeTrait) {
					case AnnotationParsing::AttributeTrait::ENUM: {
						AnnotationParsing::EnumAnnotation enumAnnotation =
						        AnnotationParsing::parseEnumAnnotation(annotationInfo.mAnnotation);

						NodeSpareParameter::addEnumParm(this, attrId, attrName, defaultValue, enumAnnotation.mOptions,
						                                parentFolders, description);
						break;
					}
					case AnnotationParsing::AttributeTrait::FILE: {
						AnnotationParsing::FileAnnotation fileAnnotation =
						        AnnotationParsing::parseFileAnnotation(annotationInfo.mAnnotation);

						NodeSpareParameter::addFileParm(this, attrId, attrName, defaultValue, fileAnnotation,
						                                parentFolders, description);
						break;
					}
					case AnnotationParsing::AttributeTrait::DIR: {
						NodeSpareParameter::addDirectoryParm(this, attrId, attrName, defaultValue, parentFolders,
						                                     description);
						break;
					}
					case AnnotationParsing::AttributeTrait::COLOR: {
						AnnotationParsing::ColorAnnotation color = AnnotationParsing::parseColor(defaultValue);

						NodeSpareParameter::addColorParm(this, attrId, attrName, color, parentFolders, description);
						break;
					}
					default: {
						NodeSpareParameter::addStringParm(this, attrId, attrName, defaultValue, parentFolders,
						                                  description);
						break;
					}
				}

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
		const PRM_Parm& parm = getParm((size_t)data);

		if (parm.isSpareParm()) {
			const PRM_Type currParmType = parm.getType();

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
