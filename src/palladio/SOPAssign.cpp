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

// clang-format off
#include "BoostRedirect.h"
#include PLD_BOOST_INCLUDE(/algorithm/string.hpp)
// clang-format on

namespace {

constexpr bool DBG = false;
constexpr const wchar_t* ENCODER_ID_CGA_EVALATTR = L"com.esri.prt.core.AttributeEvalEncoder";

constexpr const wchar_t* RULE_ATTRIBUTES_FOLDER_NAME = L"Rule Attributes";

constexpr const wchar_t* NULL_KEY = L"#NULL#";
constexpr const wchar_t* MIN_KEY = L"min";
constexpr const wchar_t* MAX_KEY = L"max";

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

enum class RangeType { RANGE, ENUM, INVALID };
RangeType GetRangeType(const prt::Annotation* an) {
	const size_t numArgs = an->getNumArguments();

	if (numArgs == 0)
		return RangeType::INVALID;

	prt::AnnotationArgumentType commonType = an->getArgument(0)->getType();

	bool hasMin = false;
	bool hasMax = false;
	bool hasKey = false;
	bool onlyOneTypeInUse = true;

	for (int argIdx = 0; argIdx < numArgs; argIdx++) {
		const prt::AnnotationArgument* arg = an->getArgument(argIdx);
		if (arg->getType() != commonType)
			onlyOneTypeInUse = false;

		const wchar_t* key = arg->getKey();
		if (std::wcscmp(key, MIN_KEY) == 0)
			hasMin = true;
		else if (std::wcscmp(key, MAX_KEY) == 0)
			hasMax = true;
		if (std::wcscmp(key, NULL_KEY) != 0)
			hasKey = true;
	}

	// old Range
	if ((numArgs == 2) && (commonType == prt::AnnotationArgumentType::AAT_FLOAT))
		return RangeType::RANGE;

	// new Range
	if ((numArgs >= 2) && (hasMin && hasMax))
		return RangeType::RANGE;

	// legacy Enum
	if (!hasKey && onlyOneTypeInUse)
		return RangeType::ENUM;

	return RangeType::INVALID;
}

std::pair<double, double> getAttributeRange(const std::wstring& attributeName, const RuleFileInfoUPtr& info) {
	auto minMax = std::make_pair(std::numeric_limits<double>::quiet_NaN(), std::numeric_limits<double>::quiet_NaN());

	for (size_t ai = 0, numAttrs = info->getNumAttributes(); ai < numAttrs; ai++) {
		const auto* attr = info->getAttribute(ai);
		if (std::wcscmp(attributeName.c_str(), attr->getName()) != 0)
			continue;

		for (size_t a = 0; a < attr->getNumAnnotations(); a++) {
			const prt::Annotation* an = attr->getAnnotation(a);
			const wchar_t* anName = an->getName();

			if (std::wcscmp(anName, ANNOT_RANGE) == 0) {
				const RangeType annotationRangeType = GetRangeType(an);
				if (annotationRangeType != RangeType::RANGE)
					continue;

				const size_t numArgs = an->getNumArguments();

				for (int argIdx = 0; argIdx < numArgs; argIdx++) {
					const prt::AnnotationArgument* arg = an->getArgument(argIdx);
					const wchar_t* key = arg->getKey();
					if (std::wcscmp(key, MIN_KEY) == 0) {
						minMax.first = arg->getFloat();
					}
					else if (std::wcscmp(key, MAX_KEY) == 0) {
						minMax.second = arg->getFloat();
					}
				}

				// parse old style range
				if ((std::isnan(minMax.first) || std::isnan(minMax.second)) && (numArgs == 2)) {
					minMax.first = an->getArgument(0)->getFloat();
					minMax.second = an->getArgument(1)->getFloat();
				}

				return minMax;
			}
		}
	}
	return minMax;
}

bool evaluateDefaultRuleAttributes(const GU_Detail* detail, ShapeData& shapeData,
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
		AttributeMapUPtr ruleAttr(amb->createAttributeMap());

		auto& isb = shapeData.getInitialShapeBuilder(isIdx);
		isb->setAttributes(ma.mRuleFile.c_str(), ma.mStartRule.c_str(), shapeData.getInitialShapeRandomSeed(isIdx),
		                   shapeName.c_str(), ruleAttr.get(), resolveMap.get());

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

void SOPAssign::updateDefaultAttributes(const ShapeData& shapeData) {
	mDefaultAttributes.clear();

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

			AttributeValueType defVal;
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
					assert(defVal.which() == 0); // std::wstring is type index 0
					break;
				}
				default:
					break;
			}
			if (!defVal.empty())
				mDefaultAttributes.emplace(key, defVal);
		}
	}
}

void SOPAssign::updateAttributes(GU_Detail* detail) {
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
					// only support booleans, i.e. don't store folders
					if (currParmType.getOrdinalType() != PRM_Type::PRM_OrdinalType::PRM_ORD_TOGGLE)
						continue;

					const int intValue = evalInt(&parm, 0, time);

					GA_RWHandleI ordinalHandle(detail->addIntTuple(attrOwner, attributeName, 1, GA_Defaults(0), nullptr, nullptr, GA_STORE_INT8));
					ordinalHandle.set(0, intValue);
					break;
				}
				case PRM_Type::PRM_BasicType::PRM_BASIC_FLOAT: {
					const double floatValue = evalFloat(&parm, 0, time);

					GA_RWHandleD floatHandle(detail->addFloatTuple(attrOwner, attributeName, 1));
					floatHandle.set(0, floatValue);
					break;
				}
				case PRM_Type::PRM_BasicType::PRM_BASIC_STRING: {
					UT_String stringValue;
					evalString(stringValue, &parm, 0, time);

					GA_RWHandleS stringHandle(detail->addStringTuple(attrOwner, attributeName, 1));
					stringHandle.set(0, stringValue);
					break;
				}
			}
		}
	}
};

void SOPAssign::refreshAttributeUI(GU_Detail* detail, ShapeData& shapeData, const ShapeConverterUPtr& shapeConverter,
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

		FolderVec parentFolders;
		parentFolders.push_back(RULE_ATTRIBUTES_FOLDER_NAME);
		parentFolders.push_back(ra.ruleFile);
		parentFolders.insert(parentFolders.end(), ra.groups.begin(), ra.groups.end());

		const auto defaultValIt = mDefaultAttributes.find(ra.fqName);
		const bool foundDefaultValue = (defaultValIt != mDefaultAttributes.end());

		switch (ra.mType) {
			case prt::AnnotationArgumentType::AAT_BOOL: {
				const bool isDefaultValBool = (defaultValIt->second.which() == 2);
				const bool defaultValue =
				        (foundDefaultValue && isDefaultValBool) ? PLD_BOOST_NS::get<bool>(defaultValIt->second) : false;
				NodeSpareParameter::addBoolParm(this, attrId, attrName, defaultValue, parentFolders);
				break;
			}
			case prt::AnnotationArgumentType::AAT_FLOAT: {
				const auto minMax = getAttributeRange(ra.fqName, ruleFileInfo);

				const bool isDefaultValFloat = (defaultValIt->second.which() == 1);
				const double defaultValue = (foundDefaultValue && isDefaultValFloat)
				                                    ? PLD_BOOST_NS::get<double>(defaultValIt->second)
				                                    : 0.0;
				NodeSpareParameter::addFloatParm(this, attrId, attrName, defaultValue, minMax.first,
				                                 minMax.second, parentFolders);
				break;
			}
			case prt::AnnotationArgumentType::AAT_STR: {
				const bool isDefaultValString = (defaultValIt->second.which() == 0);
				const std::wstring defaultValue = (foundDefaultValue && isDefaultValString)
				                                          ? PLD_BOOST_NS::get<std::wstring>(defaultValIt->second)
				                                          : L"";
				NodeSpareParameter::addStringParm(this, attrId, attrName, defaultValue, parentFolders);
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
		        evaluateDefaultRuleAttributes(gdp, shapeData, mShapeConverter, mPRTCtx, evalAttrErrorMessage);
		if (!canContinue) {
			const std::string errMsg =
			        "Could not successfully evaluate default rule attributes:\n" + evalAttrErrorMessage;
			LOG_ERR << getName() << ": " << errMsg;
			addError(SOP_MESSAGE, errMsg.c_str());
			return UT_ERROR_ABORT;
		}
		auto oldAttributes = mDefaultAttributes;
		updateDefaultAttributes(shapeData);
		if ((oldAttributes != mDefaultAttributes) && !mWasJustLoaded)
			refreshAttributeUI(gdp, shapeData, mShapeConverter, mPRTCtx, evalAttrErrorMessage);
		updateAttributes(gdp);

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
	if (reason == OP_PARM_CHANGED)
		forceRecook();
}
