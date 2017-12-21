#include "SOPGenerate.h"
#include "NodeParameter.h"
#include "ShapeGenerator.h"
#include "PrimitiveClassifier.h"
#include "ModelConverter.h"
#include "MultiWatch.h"

#include "UT/UT_Interrupt.h"

#include <future>


namespace {

constexpr bool           ENABLE_OCCLUSION     = true;

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
	const bool emitAttributes      = (evalInt(GenerateNodeParams::EMIT_ATTRS.getToken(), 0, now) > 0);
	const bool emitMaterial        = (evalInt(GenerateNodeParams::EMIT_MATERIAL.getToken(), 0, now) > 0);
	const bool emitReports         = (evalInt(GenerateNodeParams::EMIT_REPORTS.getToken(), 0, now) > 0);

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

OP_ERROR SOPGenerate::cookMySop(OP_Context& context) {
	WA("all");

	if (!handleParams(context))
		return UT_ERROR_ABORT;

	if (lockInputs(context) >= UT_ERROR_ABORT)
		return error();

	duplicateSource(0, context);

	if (error() < UT_ERROR_ABORT && cookInputGroups(context) < UT_ERROR_ABORT) {
		UT_AutoInterrupt progress("Generating CityEngine geometry...");

		ShapeData shapeData;
		ShapeGenerator shapeGen;
		shapeGen.get(gdp, DEFAULT_PRIMITIVE_CLASSIFIER, shapeData, mPRTCtx);

		const InitialShapeNOPtrVector& is = shapeData.mInitialShapes;
		if (is.empty()) {
			LOG_ERR << getName() << ": could not extract any initial shapes from detail!";
			return UT_ERROR_ABORT;
		}

		if (!progress.wasInterrupted()) {
			gdp->clearAndDestroy();
			{
				WA("generate");

				// establish threads
				const size_t nThreads = std::min<size_t>(mPRTCtx->mCores, is.size());
				const size_t isRangeSize = std::ceil(is.size() / nThreads);

				// prt requires one callback instance per generate call
				std::vector<ModelConverterUPtr> hg(nThreads);
				std::generate(hg.begin(), hg.end(), [this,&progress]() -> ModelConverterUPtr {
					return ModelConverterUPtr(new ModelConverter(gdp, &progress));
				});

				LOG_INF << getName() << ": calling generate: #initial shapes = " << is.size() << ", #threads = "
				        << nThreads << ", initial shapes per thread = " << isRangeSize;

				// kick-off generate threads
				std::vector<std::future<void>> futures;
				futures.reserve(nThreads);
				for (int8_t ti = 0; ti < nThreads; ti++) {
					auto f = std::async(std::launch::async, [this, &hg, ti, &nThreads, &isRangeSize, &is] {
						const size_t isStartPos = ti * isRangeSize;
						const size_t isPastEndPos = (ti < nThreads - 1) ? (ti + 1) * isRangeSize : is.size();
						const size_t isActualRangeSize = isPastEndPos - isStartPos;
						const auto isRangeStart = &is[isStartPos];

						LOG_DBG << "thread " << ti << ": #is = " << isActualRangeSize;

						OcclusionSetUPtr occlSet;
						std::vector<prt::OcclusionSet::Handle> occlHandles;
						if (ENABLE_OCCLUSION) {
							occlSet.reset(prt::OcclusionSet::create());
							occlHandles.resize(isActualRangeSize);
							prt::generateOccluders(isRangeStart, isActualRangeSize, occlHandles.data(), nullptr, 0,
							                       nullptr, hg[ti].get(), mPRTCtx->mPRTCache.get(), occlSet.get(),
							                       mGenerateOptions.get());
						}

						prt::Status stat = prt::generate(isRangeStart, isActualRangeSize, occlHandles.data(),
						                                 mAllEncoders.data(), mAllEncoders.size(),
						                                 mAllEncoderOptions.data(), hg[ti].get(),
						                                 mPRTCtx->mPRTCache.get(), occlSet.get(),
						                                 mGenerateOptions.get());
						if (stat != prt::STATUS_OK) {
							LOG_ERR << "prt::generate() failed with status: '"
							        << prt::getStatusDescription(stat) << "' (" << stat << ")";
						}

						if (ENABLE_OCCLUSION)
							occlSet->dispose(occlHandles.data(), occlHandles.size());
					});
					futures.emplace_back(std::move(f));
				}
				std::for_each(futures.begin(), futures.end(), [](std::future<void>& f) { f.wait(); });
			}
			select();
		}
	}

	unlockInputs();

	WA_PRINT_TIMINGS

	return error();
}
