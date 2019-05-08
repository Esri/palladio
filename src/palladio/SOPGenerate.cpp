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

#include "SOPGenerate.h"
#include "NodeParameter.h"
#include "ShapeGenerator.h"
#include "ShapeData.h"
#include "PrimitiveClassifier.h"
#include "ModelConverter.h"
#include "MultiWatch.h"

#include "UT/UT_Interrupt.h"

#include <future>
#include <algorithm>


namespace {

constexpr const wchar_t* FILE_CGA_ERROR       = L"CGAErrors.txt";
constexpr const wchar_t* FILE_CGA_PRINT       = L"CGAPrint.txt";

constexpr const wchar_t* ENCODER_ID_CGA_ERROR = L"com.esri.prt.core.CGAErrorEncoder";
constexpr const wchar_t* ENCODER_ID_CGA_PRINT = L"com.esri.prt.core.CGAPrintEncoder";

const PrimitiveClassifier DEFAULT_PRIMITIVE_CLASSIFIER;

} // namespace


SOPGenerate::SOPGenerate(const PRTContextUPtr& pCtx, OP_Network* net, const char* name, OP_Operator* op)
: SOP_Node(net, name, op), mPRTCtx(pCtx)
{
	AttributeMapBuilderUPtr optionsBuilder(prt::AttributeMapBuilder::create());

	optionsBuilder->setString(L"name", FILE_CGA_ERROR);
	const AttributeMapUPtr errOptions(optionsBuilder->createAttributeMapAndReset());
	mCGAErrorOptions.reset(createValidatedOptions(ENCODER_ID_CGA_ERROR, errOptions.get()));

	optionsBuilder->setString(L"name", FILE_CGA_PRINT);
	const AttributeMapUPtr printOptions(optionsBuilder->createAttributeMapAndReset());
	mCGAPrintOptions.reset(createValidatedOptions(ENCODER_ID_CGA_PRINT, printOptions.get()));

	AttributeMapBuilderUPtr amb(prt::AttributeMapBuilder::create());
	amb->setInt(L"numberWorkerThreads", mPRTCtx->mCores);
	mGenerateOptions.reset(amb->createAttributeMapAndReset());
}

bool SOPGenerate::handleParams(OP_Context& context) {
	const auto now = context.getTime();
	const bool emitAttributes = (evalInt(GenerateNodeParams::EMIT_ATTRS.getToken(), 0, now) > 0);
	const bool emitMaterial   = (evalInt(GenerateNodeParams::EMIT_MATERIAL.getToken(), 0, now) > 0);
	const bool emitReports    = (evalInt(GenerateNodeParams::EMIT_REPORTS.getToken(), 0, now) > 0);

	AttributeMapBuilderUPtr optionsBuilder(prt::AttributeMapBuilder::create());
	optionsBuilder->setBool(EO_EMIT_ATTRIBUTES, emitAttributes);
	optionsBuilder->setBool(EO_EMIT_MATERIALS, emitMaterial);
	optionsBuilder->setBool(EO_EMIT_REPORTS, emitReports);
	AttributeMapUPtr encoderOptions(optionsBuilder->createAttributeMapAndReset());
	mHoudiniEncoderOptions.reset(createValidatedOptions(ENCODER_ID_HOUDINI, encoderOptions.get()));
	if (!mHoudiniEncoderOptions)
		return false;

	mAllEncoders = { ENCODER_ID_HOUDINI, ENCODER_ID_CGA_ERROR, ENCODER_ID_CGA_PRINT };
	mAllEncoderOptions = { mHoudiniEncoderOptions.get(), mCGAErrorOptions.get(), mCGAPrintOptions.get() };

	return true;
}

namespace {

enum class BatchMode { OCCLUSION, GENERATION };
const std::vector<std::string> BATCH_MODE_NAMES = { "occlusion", "generation" };

std::vector<prt::Status> batchGenerate(BatchMode mode,
                                       size_t nThreads,
                                       std::vector<ModelConverterUPtr>& hg,
                                       size_t isRangeSize,
                                       const InitialShapeNOPtrVector& is,
                                       const std::vector<const wchar_t*>& allEncoders,
                                       const AttributeMapNOPtrVector& allEncoderOptions,
                                       std::vector<prt::OcclusionSet::Handle>& occlusionHandles,
                                       OcclusionSetUPtr& occlusionSet,
                                       CacheObjectUPtr& prtCache,
                                       const AttributeMapUPtr& genOpts)
{
	std::vector<prt::Status> batchStatus(nThreads, prt::STATUS_UNSPECIFIED_ERROR);

	std::vector<std::future<void>> futures;
	futures.reserve(nThreads);
	for (int8_t ti = 0; ti < nThreads; ti++) {
		auto f = std::async(std::launch::async, [&,ti] { // capture thread index by value, else we have is range chaos
			const size_t isStartPos = ti * isRangeSize;
			const size_t isPastEndPos = (ti < nThreads - 1) ? (ti + 1) * isRangeSize : is.size();
			const size_t isActualRangeSize = isPastEndPos - isStartPos;
			const auto isRangeStart = &is[isStartPos];
			const auto isOcclRangeStart = &occlusionHandles[isStartPos];

			LOG_DBG << "thread " << ti << ": #is = " << isActualRangeSize;

			switch (mode) {
				case BatchMode::OCCLUSION: {
					batchStatus[ti] = prt::generateOccluders(isRangeStart, isActualRangeSize, isOcclRangeStart,
					                                         nullptr, 0, nullptr, hg[ti].get(), prtCache.get(),
					                                         occlusionSet.get(), genOpts.get());
					break;
				}
				case BatchMode::GENERATION: {
					batchStatus[ti] = prt::generate(isRangeStart, isActualRangeSize, isOcclRangeStart,
					                                allEncoders.data(), allEncoders.size(), allEncoderOptions.data(),
					                                hg[ti].get(), prtCache.get(), occlusionSet.get(), genOpts.get());
					break;
				}
			}

			if (batchStatus[ti] != prt::STATUS_OK) {
			    LOG_WRN << "batch mode " << BATCH_MODE_NAMES[(int)mode] << " failed with status: '"
			            << prt::getStatusDescription(batchStatus[ti]) << "' ("
			            << batchStatus[ti] << ")";
			}

		});
		futures.emplace_back(std::move(f));
	}
	std::for_each(futures.begin(), futures.end(), [](std::future<void>& f) { f.wait(); });

	return batchStatus;
}

} // namespace

OP_ERROR SOPGenerate::cookMySop(OP_Context& context) {
	WA("all");

	if (!handleParams(context))
		return UT_ERROR_ABORT;

	if (lockInputs(context) >= UT_ERROR_ABORT)
		return error();

	duplicateSource(0, context);

	if (error() >= UT_ERROR_ABORT || cookInputGroups(context) >= UT_ERROR_ABORT)
		return error();

	UT_AutoInterrupt progress("Generating CityEngine geometry...");

	const auto groupCreation = GenerateNodeParams::getGroupCreation(this, context.getTime());
	ShapeData shapeData(groupCreation, toUTF16FromOSNarrow(getName().toStdString()));

	ShapeGenerator shapeGen;
	shapeGen.get(gdp, DEFAULT_PRIMITIVE_CLASSIFIER, shapeData, mPRTCtx);

	const InitialShapeNOPtrVector& is = shapeData.getInitialShapes();
	if (is.empty()) {
		LOG_ERR << getName() << ": could not extract any initial shapes from detail!";
		return UT_ERROR_ABORT;
	}

	// establish threads
	const size_t nThreads = std::min<size_t>(mPRTCtx->mCores, is.size());
	const size_t isRangeSize = std::ceil(is.size() / nThreads);

	// prepare generate status receivers
	std::vector<prt::Status> initialShapeStatus(is.size(), prt::STATUS_OK);

	if (!progress.wasInterrupted()) {
		gdp->clearAndDestroy();
		{
			WA("generate");

			// prt requires one callback instance per generate call
			std::vector<ModelConverterUPtr> hg(nThreads);
			std::generate(hg.begin(), hg.end(), [this, &groupCreation, &initialShapeStatus, &progress]() -> ModelConverterUPtr {
				return ModelConverterUPtr(new ModelConverter(gdp, groupCreation, initialShapeStatus, &progress));
			});

			std::vector<prt::OcclusionSet::Handle> occlusionHandles(is.size());
			OcclusionSetUPtr occlusionSet{prt::OcclusionSet::create()};

			LOG_INF << getName() << ": calling generate: #initial shapes = " << is.size() << ", #threads = "
			        << nThreads << ", initial shapes per thread = " << isRangeSize;

			batchGenerate(BatchMode::OCCLUSION, nThreads, hg, isRangeSize, is, mAllEncoders, mAllEncoderOptions,
			              occlusionHandles, occlusionSet, mPRTCtx->mPRTCache, mGenerateOptions);

			batchGenerate(BatchMode::GENERATION, nThreads, hg, isRangeSize, is, mAllEncoders, mAllEncoderOptions,
			              occlusionHandles, occlusionSet, mPRTCtx->mPRTCache, mGenerateOptions);

			occlusionSet->dispose(occlusionHandles.data(), occlusionHandles.size());
		}
		select();
	}

	WA_PRINT_TIMINGS

	unlockInputs();

	// generate status check: if all shapes fail, we abort cooking (failure of individual shapes is sometimes expected)
	const size_t isSuccesses = std::count(initialShapeStatus.begin(), initialShapeStatus.end(), prt::STATUS_OK);
	if (isSuccesses == 0) {
		LOG_ERR << getName() << ": All initial shapes failed to generate, cooking aborted.";
		addError(UT_ERROR_ABORT, "All initial shapes failed to generate.");
		return UT_ERROR_ABORT;
	}
	// TODO: evaluate batchStatus as well...

	return error();
}

void SOPGenerate::opChanged(OP_EventType reason, void *data) {
   SOP_Node::opChanged(reason, data);

   // trigger recook on name change, we use the node name in various output places
   if (reason == OP_NAME_CHANGED)
	   forceRecook();
}
