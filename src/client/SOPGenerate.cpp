#include "SOPGenerate.h"
#include "NodeParameter.h"
#include "ShapeGenerator.h"
#include "ModelConverter.h"

#include "UT/UT_Interrupt.h"

#include <future>


namespace {

constexpr bool ENABLE_OCCLUSION = true;

constexpr const wchar_t* FILE_CGA_ERROR       = L"CGAErrors.txt";
constexpr const wchar_t* FILE_CGA_PRINT       = L"CGAPrint.txt";

constexpr const wchar_t* ENCODER_ID_CGA_ERROR = L"com.esri.prt.core.CGAErrorEncoder";
constexpr const wchar_t* ENCODER_ID_CGA_PRINT = L"com.esri.prt.core.CGAPrintEncoder";
constexpr const wchar_t* ENCODER_ID_HOUDINI   = L"HoudiniEncoder";

} // namespace


SOPGenerate::SOPGenerate(const PRTContextUPtr& pCtx, OP_Network* net, const char* name, OP_Operator* op)
: SOP_Node(net, name, op), mPRTCtx(pCtx)
{
	AttributeMapBuilderUPtr optionsBuilder(prt::AttributeMapBuilder::create());

	optionsBuilder->setString(L"name", FILE_CGA_ERROR);
	const prt::AttributeMap* errOptions = optionsBuilder->createAttributeMapAndReset();
	mCGAErrorOptions.reset(createValidatedOptions(ENCODER_ID_CGA_ERROR, errOptions));
	errOptions->destroy();

	optionsBuilder->setString(L"name", FILE_CGA_PRINT);
	const prt::AttributeMap* printOptions = optionsBuilder->createAttributeMapAndReset();
	mCGAPrintOptions.reset(createValidatedOptions(ENCODER_ID_CGA_PRINT, printOptions));
	printOptions->destroy();

	AttributeMapBuilderUPtr amb(prt::AttributeMapBuilder::create());
	amb->setInt(L"numberWorkerThreads", mPRTCtx->mCores);
	mGenerateOptions.reset(amb->createAttributeMapAndReset());
}

bool SOPGenerate::handleParams(OP_Context& context) {
	const auto now = context.getTime();
	const bool emitAttributes      = (evalInt(GenerateNodeParams::EMIT_ATTRS.getToken(), 0, now) > 0);
	const bool emitMaterial        = (evalInt(GenerateNodeParams::EMIT_MATERIAL.getToken(), 0, now) > 0);
	const bool emitReports         = (evalInt(GenerateNodeParams::EMIT_REPORTS.getToken(), 0, now) > 0);
	const bool emitReportSummaries = (evalInt(GenerateNodeParams::EMIT_REPORT_SUMMARIES.getToken(), 0, now) > 0);

	AttributeMapBuilderUPtr optionsBuilder(prt::AttributeMapBuilder::create());
	optionsBuilder->setBool(L"emitAttributes", emitAttributes);
	optionsBuilder->setBool(L"emitMaterials", emitMaterial);
	optionsBuilder->setBool(L"emitReports", emitReports);
	optionsBuilder->setBool(L"emitReportSummaries", emitReportSummaries);
	AttributeMapUPtr encoderOptions(optionsBuilder->createAttributeMapAndReset());
	mHoudiniEncoderOptions.reset(createValidatedOptions(ENCODER_ID_HOUDINI, encoderOptions.get()));

	mAllEncoders = { ENCODER_ID_HOUDINI, ENCODER_ID_CGA_ERROR, ENCODER_ID_CGA_PRINT };
	mAllEncoderOptions = { mHoudiniEncoderOptions.get(), mCGAErrorOptions.get(), mCGAPrintOptions.get() };

	return true;
}

OP_ERROR SOPGenerate::cookMySop(OP_Context& context) {
	std::chrono::time_point<std::chrono::system_clock> start, end;

	if (!handleParams(context))
		return UT_ERROR_ABORT;

	if (lockInputs(context) >= UT_ERROR_ABORT)
		return error();

	duplicateSource(0, context);

	if (error() < UT_ERROR_ABORT && cookInputGroups(context) < UT_ERROR_ABORT) {
		UT_AutoInterrupt progress("Generating CityEngine geometry...");

		ShapeData shapeData;
		ShapeGenerator shapeGen;

		start = std::chrono::system_clock::now();
		shapeGen.get(gdp, shapeData, mPRTCtx);
		end = std::chrono::system_clock::now();
		LOG_INF << getName() << ": creating initial shapes from detail: " << std::chrono::duration<double>(end - start).count() << "s\n";

		if (!progress.wasInterrupted()) {
			gdp->clearAndDestroy();
			{
				// establish threads
				const InitialShapeNOPtrVector& is = shapeData.mInitialShapes;
				const size_t nThreads = std::min<size_t>(mPRTCtx->mCores, is.size());
				const size_t isRangeSize = std::ceil(is.size() / nThreads);

				// prt requires one callback instance per generate call
				std::vector<std::unique_ptr<ModelConverter>> hg(nThreads);
				std::generate(hg.begin(), hg.end(), [&]() {
						return std::unique_ptr<ModelConverter>(new ModelConverter(gdp, &progress));
					}
				);

				LOG_INF << getName() << ": calling generate: #initial shapes = " << is.size() << ", #threads = " << nThreads << ", initial shapes per thread = " << isRangeSize;

				// kick-off generate threads
				start = std::chrono::system_clock::now();
				std::vector<std::future<void>> futures;
				futures.reserve(nThreads);
				for (int8_t ti = 0; ti < nThreads; ti++) {
					auto f = std::async(
							std::launch::async, [this, &hg, ti, &nThreads, &isRangeSize, &is] {
								size_t isStartPos = ti * isRangeSize;
								size_t isPastEndPos = (ti < nThreads - 1) ? (ti + 1) * isRangeSize : is.size();
								size_t isActualRangeSize = isPastEndPos - isStartPos;
								auto isRangeStart = &is[isStartPos];

								LOG_INF << "thread " << ti << ": #is = " << isActualRangeSize;

								OcclusionSetUPtr occlSet;
								std::vector<prt::OcclusionSet::Handle> occlHandles;
								if (ENABLE_OCCLUSION) {
									occlSet.reset(prt::OcclusionSet::create());
									occlHandles.resize(isActualRangeSize);
									prt::generateOccluders(
											isRangeStart, isActualRangeSize, occlHandles.data(), nullptr, 0,
											nullptr, hg[ti].get(), mPRTCtx->mPRTCache.get(), occlSet.get(),
											mGenerateOptions.get()
									);
								}

								prt::Status stat = prt::generate(
										isRangeStart, isActualRangeSize, occlHandles.data(),
										mAllEncoders.data(), mAllEncoders.size(), mAllEncoderOptions.data(),
										hg[ti].get(), mPRTCtx->mPRTCache.get(), occlSet.get(), mGenerateOptions.get()
								);
								if (stat != prt::STATUS_OK) {
									LOG_ERR << "prt::generate() failed with status: '" <<
									prt::getStatusDescription(stat) << "' (" << stat << ")";
								}

								if (ENABLE_OCCLUSION)
									occlSet->dispose(occlHandles.data(), occlHandles.size());
							}
					);
					futures.emplace_back(std::move(f));
				}
				std::for_each(futures.begin(), futures.end(), [](std::future<void>& f) { f.wait(); });
				end = std::chrono::system_clock::now();
				LOG_INF << "generate took " << std::chrono::duration<double>(end - start).count() << "s";
			}
			select();
		}
	}

	unlockInputs();

	return error();
}
