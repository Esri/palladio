#include "client/SOPGenerate.h"
#include "client/callbacks.h"

#include "UT/UT_Interrupt.h"

#include <future>


namespace {

const wchar_t*	FILE_CGA_ERROR			= L"CGAErrors.txt";
const wchar_t*	FILE_CGA_PRINT			= L"CGAPrint.txt";

const wchar_t*	ENCODER_ID_CGA_ERROR	= L"com.esri.prt.core.CGAErrorEncoder";
const wchar_t*	ENCODER_ID_CGA_PRINT	= L"com.esri.prt.core.CGAPrintEncoder";
const wchar_t*	ENCODER_ID_HOUDINI		= L"HoudiniEncoder";

} // namespace


namespace p4h {

SOPGenerate::SOPGenerate(const PRTContextUPtr& pCtx, OP_Network* net, const char* name, OP_Operator* op)
: SOP_Node(net, name, op)
, mPRTCtx(pCtx)
{
	AttributeMapBuilderPtr optionsBuilder(prt::AttributeMapBuilder::create());

	const prt::AttributeMap* encoderOptions = optionsBuilder->createAttributeMapAndReset();
	mHoudiniEncoderOptions.reset(utils::createValidatedOptions(ENCODER_ID_HOUDINI, encoderOptions));
	encoderOptions->destroy();

	optionsBuilder->setString(L"name", FILE_CGA_ERROR);
	const prt::AttributeMap* errOptions = optionsBuilder->createAttributeMapAndReset();
	mCGAErrorOptions.reset(utils::createValidatedOptions(ENCODER_ID_CGA_ERROR, errOptions));
	errOptions->destroy();

	optionsBuilder->setString(L"name", FILE_CGA_PRINT);
	const prt::AttributeMap* printOptions = optionsBuilder->createAttributeMapAndReset();
	mCGAPrintOptions.reset(utils::createValidatedOptions(ENCODER_ID_CGA_PRINT, printOptions));
	printOptions->destroy();

#ifdef WIN32
	mAllEncoders = boost::assign::list_of(ENCODER_ID_HOUDINI)(ENCODER_ID_CGA_ERROR)(ENCODER_ID_CGA_PRINT);
	mAllEncoderOptions = boost::assign::list_of(mHoudiniEncoderOptions.get())(mCGAErrorOptions.get())(mCGAPrintOptions.get());
#else
	mAllEncoders = {ENCODER_ID_HOUDINI, ENCODER_ID_CGA_ERROR, ENCODER_ID_CGA_PRINT};
	mAllEncoderOptions = {mHoudiniEncoderOptions.get(), mCGAErrorOptions.get(), mCGAPrintOptions.get()};
#endif

	AttributeMapBuilderPtr amb(prt::AttributeMapBuilder::create());
	amb->setInt(L"numberWorkerThreads", mPRTCtx->mCores);
	mGenerateOptions.reset(amb->createAttributeMapAndReset());
}

SOPGenerate::~SOPGenerate() {
}

OP_ERROR SOPGenerate::cookMySop(OP_Context& context) {
	LOG_DBG << "SOPGenerate::cookMySop";

	if (lockInputs(context) >= UT_ERROR_ABORT) {
		LOG_DBG << "lockInputs error";
		return error();
	}
	duplicateSource(0, context);

	if (error() < UT_ERROR_ABORT && cookInputGroups(context) < UT_ERROR_ABORT) {
		UT_AutoInterrupt progress("Generating CityEngine geometry...");

		std::chrono::time_point<std::chrono::system_clock> start, end;

		InitialShapeContext isCtx(gdp);

		start = std::chrono::system_clock::now();
		InitialShapeGenerator isGen(gdp, isCtx);
		end = std::chrono::system_clock::now();
		LOG_INF << "InitialShapeGenerator took " << std::chrono::duration<double>(end - start).count() << "s\n";

		if (!progress.wasInterrupted()) {
			gdp->clearAndDestroy();
			{
				InitialShapeNOPtrVector& is = isGen.getInitialShapes();

				// establish threads
				const uint32_t nThreads = 2 * mPRTCtx->mCores; // TODO: find good thread load
				const uint32_t isRangeSize = std::ceil(is.size() / nThreads);

				// prt requires one callback instance per generate call
				std::vector<std::unique_ptr<HoudiniGeometry>> hg(nThreads);
				std::generate(
						hg.begin(), hg.end(), [&]() {
							return std::unique_ptr<HoudiniGeometry>(new HoudiniGeometry(gdp, nullptr, &progress));
						}
				);

				LOG_INF << "calling generate: #initial shapes = " << is.size()
				<< ", #threads = " << nThreads << ", initial shapes per thread = " << isRangeSize;

				// kick-off generate threads
				start = std::chrono::system_clock::now();
				std::vector<std::future<void>> futures;
				for (int8_t ti = 0; ti < nThreads; ti++) {
					auto f = std::async(
							std::launch::async, [this, &hg, ti, &nThreads, &isRangeSize, &is] {
								size_t isStartPos = ti * isRangeSize;
								size_t isPastEndPos = (ti < nThreads - 1) ? (ti + 1) * isRangeSize : is.size();
								size_t isActualRangeSize = isPastEndPos - isStartPos;
								auto isRangeStart = &is[isStartPos];

								LOG_INF << "thread " << ti << ": #is = " << isActualRangeSize;

								// TODO: add occlusion neighborhood!!!
								OcclusionSetPtr occlSet(prt::OcclusionSet::create());
								std::vector<prt::OcclusionSet::Handle> occlHandles(isActualRangeSize);
								prt::generateOccluders(
										isRangeStart, isActualRangeSize, occlHandles.data(), nullptr, 0,
										nullptr, hg[ti].get(), mPRTCtx->mPRTCache.get(), occlSet.get(), mGenerateOptions.get()
								);

								prt::Status stat = prt::generate(
										isRangeStart, isActualRangeSize, 0,
										mAllEncoders.data(), mAllEncoders.size(), mAllEncoderOptions.data(),
										hg[ti].get(), mPRTCtx->mPRTCache.get(), occlSet.get(), mGenerateOptions.get()
								);
								if (stat != prt::STATUS_OK) {
									LOG_ERR << "prt::generate() failed with status: '" <<
									prt::getStatusDescription(stat) << "' (" << stat << ")";
								}

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

	LOG_DBG << "SOPGenerate::cookMySop done";
	return error();
}

} // namespace p4h
