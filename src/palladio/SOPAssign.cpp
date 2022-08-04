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
#include "AnnotationParsing.h"
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

bool compareAttributeTypes(const NodeSpareParameter::SpareParmVec& refDefaultValues,
                           const NodeSpareParameter::SpareParmVec& newDefaultValues) {
	if (refDefaultValues.size() != newDefaultValues.size())
		return false;
	
	for (size_t spareParmIdx = 0; spareParmIdx < refDefaultValues.size(); ++spareParmIdx) {
		const auto& refValue = refDefaultValues[spareParmIdx];
		const auto& newValue = newDefaultValues[spareParmIdx];
		if ((newValue->tokenId != newValue->tokenId) || (newValue->type != refValue->type))
				return false;
	}
	return true;
}

void updateUIDefaultValues(SOPAssign* node, const NodeSpareParameter::SpareParmVec& spareParms, ShapeData& shapeData) {
	AttributeMapVector defaultRuleAttributeMaps;
	for (auto& amb : shapeData.getRuleAttributeMapBuilders()) {
		defaultRuleAttributeMaps.emplace_back(amb->createAttributeMap());
	}

	for (const auto& spareParm : spareParms) {
		spareParm->replaceDefaultValueFromMap(node, defaultRuleAttributeMaps);
	}
}

AttributeMapUPtr generateAttributeMapFromParameterValues(SOPAssign* node,
                                                         const NodeSpareParameter::SpareParmVec& spareParms) {
	AttributeMapBuilderUPtr amb(prt::AttributeMapBuilder::create());
	for (auto& spareParm : spareParms) {
		spareParm->addCurrentValueToAttrMap(node, amb);
	}
	return AttributeMapUPtr(amb->createAttributeMap());
}

bool evaluateDefaultRuleAttributes(SOPAssign* node, const GU_Detail* detail, ShapeData& shapeData,
                                   const NodeSpareParameter::SpareParmVec& spareParms,
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
		AttributeMapUPtr ruleAttr = generateAttributeMapFromParameterValues(node, spareParms);

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

void SOPAssign::updateCGAAttributes(GU_Detail* detail, ShapeData& shapeData, const ShapeConverterUPtr& shapeConverter,
                                    const PRTContextUPtr& prtCtx, std::string& errors) {
	mSpareParms.clear();

	AttributeMapVector defaultRuleAttributeMaps;
	for (auto& amb : shapeData.getRuleAttributeMapBuilders()) {
		defaultRuleAttributeMaps.emplace_back(amb->createAttributeMap());
	}
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

	mSpareParms =
	        NodeSpareParameter::createSpareParmsFromAttrInfo(defaultRuleAttributeMaps, ruleFileInfo, ruleAttributes);
}

void SOPAssign::updatePrimitiveAttributes(GU_Detail* detail) {
	for (auto& spareParm : mSpareParms) {
		spareParm->addPrimitiveAttributeToNode(this, detail);
	}
};

void SOPAssign::buildUI() {
	NodeSpareParameter::clearAllParms(this);
	NodeSpareParameter::addSeparator(this);
	NodeSpareParameter::addSimpleFolder(this, NodeSpareParameter::RULE_ATTRIBUTES_FOLDER_NAME);

	for (const auto& spareParm : mSpareParms) {
		spareParm->addToNode(this);
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
		        evaluateDefaultRuleAttributes(this, gdp, shapeData, mSpareParms, mShapeConverter, mPRTCtx, evalAttrErrorMessage);
		if (!canContinue) {
			const std::string errMsg =
			        "Could not successfully evaluate default rule attributes:\n" + evalAttrErrorMessage;
			LOG_ERR << getName() << ": " << errMsg;
			addError(SOP_MESSAGE, errMsg.c_str());
			return UT_ERROR_ABORT;
		}
		auto oldAttributes = mSpareParms;
		updateCGAAttributes(gdp, shapeData, mShapeConverter, mPRTCtx, evalAttrErrorMessage);
		if (!compareAttributeTypes(oldAttributes, mSpareParms) && !mWasJustLoaded)
			buildUI();
		updateUIDefaultValues(this, mSpareParms, shapeData);
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
