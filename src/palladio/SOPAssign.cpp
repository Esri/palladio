/*
 * Copyright 2014-2019 Esri R&D Zurich and VRBN
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
#include "PrimitiveClassifier.h"
#include "ShapeGenerator.h"
#include "ShapeData.h"
#include "ModelConverter.h"
#include "NodeParameter.h"
#include "LogHandler.h"
#include "MultiWatch.h"

#include "prt/API.h"

#include "UT/UT_Interrupt.h"

#include "BoostRedirect.h"
#include PLD_BOOST_INCLUDE(/algorithm/string.hpp)


namespace {

constexpr bool           DBG                     = false;
constexpr const wchar_t* ENCODER_ID_CGA_EVALATTR = L"com.esri.prt.core.AttributeEvalEncoder";

AttributeMapUPtr getValidEncoderInfo(const wchar_t* encID) {
	const EncoderInfoUPtr encInfo(prt::createEncoderInfo(encID));
	const prt::AttributeMap* encOpts = nullptr;
	encInfo->createValidatedOptionsAndStates(nullptr, &encOpts);
	return AttributeMapUPtr(encOpts);
}

RuleFileInfoUPtr getRuleFileInfo(const MainAttributes& ma, const ResolveMapUPtr& resolveMap, prt::Cache* prtCache) {
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

bool evaluateDefaultRuleAttributes(
		const GU_Detail* detail,
		ShapeData& shapeData,
		const ShapeConverterUPtr& shapeConverter,
		const PRTContextUPtr& prtCtx
) {
	WA("all");

	assert(shapeData.isValid());

	// setup encoder options for attribute evaluation encoder
	constexpr const wchar_t* encs[] = { ENCODER_ID_CGA_EVALATTR };
	constexpr size_t encsCount = sizeof(encs) / sizeof(encs[0]);
	const AttributeMapUPtr encOpts = getValidEncoderInfo(ENCODER_ID_CGA_EVALATTR);
	const prt::AttributeMap* encsOpts[] = { encOpts.get() };

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
		const ResolveMapUPtr& resolveMap = prtCtx->getResolveMap(ma.mRPK);
		if (!resolveMap) {
			LOG_WRN << "Could not create resolve map from rpk " << ma.mRPK << ", aborting default rule attribute evaluation";
			return false;
		}

		ruleFileInfos[isIdx] = getRuleFileInfo(ma, resolveMap, prtCtx->mPRTCache.get());

		const std::wstring shapeName = L"shape_" + std::to_wstring(isIdx);
		if (DBG) LOG_DBG << "evaluating attrs for shape: " << shapeName;

		// persist rule attributes even if empty (need to live until prt::generate is done)
		AttributeMapBuilderUPtr amb(prt::AttributeMapBuilder::create());
		AttributeMapUPtr ruleAttr(amb->createAttributeMap());

		auto& isb = shapeData.getInitialShapeBuilder(isIdx);
		isb->setAttributes(
				ma.mRuleFile.c_str(),
				ma.mStartRule.c_str(),
				shapeData.getInitialShapeRandomSeed(isIdx),
				shapeName.c_str(),
				ruleAttr.get(),
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
		LOG_ERR << "assign: prt::generate() failed with status: '" << prt::getStatusDescription(stat) << "' (" << stat << ")";
	}

	assert(shapeData.isValid());

	return true;
}

} // namespace


SOPAssign::SOPAssign(const PRTContextUPtr& pCtx, OP_Network* net, const char* name, OP_Operator* op)
: SOP_Node(net, name, op), mPRTCtx(pCtx), mShapeConverter(new ShapeConverter()) { }

OP_ERROR SOPAssign::cookMySop(OP_Context& context) {
	WA_NEW_LAP
	WA("all");

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
		const bool canContinue = evaluateDefaultRuleAttributes(gdp, shapeData, mShapeConverter, mPRTCtx);
		if (!canContinue) {
			LOG_ERR << getName() << ": aborting, could not successfully evaluate default rule attributes";
			return UT_ERROR_ABORT;
		}
		mShapeConverter->put(gdp, primCls, shapeData);
	}

	unlockInputs();

	return error();
}

void SOPAssign::opChanged(OP_EventType reason, void *data) {
   SOP_Node::opChanged(reason, data);

   // trigger recook on name change, we use the node name in various output places
   if (reason == OP_NAME_CHANGED)
	   forceRecook();
}
