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

#include "TestUtils.h"

#include "prt/API.h"

#include "catch.hpp"

namespace {

constexpr const wchar_t* ENCODER_ID_CGA_ERROR = L"com.esri.prt.core.CGAErrorEncoder";
constexpr const wchar_t* ENCODER_ID_CGA_PRINT = L"com.esri.prt.core.CGAPrintEncoder";
constexpr const wchar_t* FILE_CGA_ERROR = L"CGAErrors.txt";
constexpr const wchar_t* FILE_CGA_PRINT = L"CGAPrint.txt";

} // namespace

void generate(TestCallbacks& tc, const PRTContextUPtr& prtCtx, const std::filesystem::path& rpkPath,
              const std::wstring& ruleFile, const std::vector<std::wstring>& initialShapeURIs,
              const std::vector<std::wstring>& startRules) {
	REQUIRE(initialShapeURIs.size() == startRules.size());

	ResolveMapSPtr rpkRM = prtCtx->getResolveMap(rpkPath);

	std::vector<std::pair<std::wstring, std::wstring>> cgbs; // key -> uri
	getCGBs(rpkRM, cgbs);

	AttributeMapBuilderUPtr amb(prt::AttributeMapBuilder::create());
	const RuleFileInfoUPtr ruleFileInfo(prt::createRuleFileInfo(cgbs[0].second.c_str(), prtCtx->mPRTCache.get()));
	for (size_t ai = 0, numAttrs = ruleFileInfo->getNumAttributes(); ai < numAttrs; ai++) {
		const auto* a = ruleFileInfo->getAttribute(ai);
		switch (a->getReturnType()) {
			case prt::AAT_BOOL:
				amb->setBool(a->getName(), false);
				break;
			case prt::AAT_FLOAT:
				amb->setFloat(a->getName(), 0.0);
				break;
			case prt::AAT_STR:
				amb->setString(a->getName(), L"");
				break;
			case prt::AAT_INT:
				amb->setInt(a->getName(), 0);
				break;
			default:
				break;
		}
	}
	const AttributeMapUPtr isAttrsProto(amb->createAttributeMapAndReset());

	GenerateData gd;
	for (size_t i = 0; i < initialShapeURIs.size(); i++) {
		const std::wstring& uri = initialShapeURIs[i];
		const std::wstring& sr = startRules[i];

		gd.mInitialShapeBuilders.emplace_back(prt::InitialShapeBuilder::create());
		auto& isb = gd.mInitialShapeBuilders.back();

		gd.mRuleAttributeBuilders.emplace_back(
		        prt::AttributeMapBuilder::createFromAttributeMap(isAttrsProto.get())); // TODO: use shared_ptr
		const auto& rab = gd.mRuleAttributeBuilders.back();
		gd.mRuleAttributes.emplace_back(rab->createAttributeMap());
		const auto& am = gd.mRuleAttributes.back();

		const std::wstring sn = L"shape" + std::to_wstring(i);

		REQUIRE(isb->resolveGeometry(uri.c_str()) == prt::STATUS_OK);
		REQUIRE(isb->setAttributes(ruleFile.c_str(), sr.c_str(), 0, sn.c_str(), am.get(), rpkRM.get()) ==
		        prt::STATUS_OK);

		prt::Status status = prt::STATUS_UNSPECIFIED_ERROR;
		const prt::InitialShape* is = isb->createInitialShapeAndReset(&status);
		REQUIRE(status == prt::STATUS_OK);
		gd.mInitialShapes.emplace_back(is);
	}

	amb->setBool(L"emitAttributes", true);
	amb->setBool(L"emitMaterials", true);
	amb->setBool(L"emitReports", true);
	const AttributeMapUPtr rawEncOpts(amb->createAttributeMapAndReset());
	const AttributeMapUPtr houdiniEncOpts(createValidatedOptions(ENCODER_ID_HOUDINI, rawEncOpts.get()));

	amb->setString(L"name", FILE_CGA_ERROR);
	const AttributeMapUPtr errOptions(amb->createAttributeMapAndReset());
	const AttributeMapUPtr cgaErrorOptions(createValidatedOptions(ENCODER_ID_CGA_ERROR, errOptions.get()));

	amb->setString(L"name", FILE_CGA_PRINT);
	const AttributeMapUPtr printOptions(amb->createAttributeMapAndReset());
	const AttributeMapUPtr cgaPrintOptions(createValidatedOptions(ENCODER_ID_CGA_PRINT, printOptions.get()));

	amb->setInt(L"numberWorkerThreads", prtCtx->mCores);
	const AttributeMapUPtr generateOptions(amb->createAttributeMapAndReset());

	const std::vector<const wchar_t*> allEncoders = {ENCODER_ID_HOUDINI, ENCODER_ID_CGA_ERROR, ENCODER_ID_CGA_PRINT};
	const AttributeMapNOPtrVector allEncoderOptions = {houdiniEncOpts.get(), cgaErrorOptions.get(),
	                                                   cgaPrintOptions.get()};

	prt::Status stat = prt::generate(gd.mInitialShapes.data(), gd.mInitialShapes.size(), nullptr, allEncoders.data(),
	                                 allEncoders.size(), allEncoderOptions.data(), &tc, prtCtx->mPRTCache.get(),
	                                 nullptr, generateOptions.get());
	REQUIRE(stat == prt::STATUS_OK);
}
