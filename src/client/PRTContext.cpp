#include "PRTContext.h"
#include "LogHandler.h"

#include <thread>


namespace {

constexpr const char*         PRT_LIB_SUBDIR          = "prtlib";
constexpr const char*         FILE_FLEXNET_LIB        = "flexnet_prt";

constexpr const prt::LogLevel PRT_LOG_LEVEL_DEFAULT   = prt::LOG_ERROR;
constexpr const char*         PRT_LOG_LEVEL_ENV_VAR   = "CITYENGINE_LOG_LEVEL";
constexpr const char*         PRT_LOG_LEVEL_STRINGS[] = { "trace", "debug", "info", "warning", "error", "fatal" };
constexpr const size_t        PRT_LOG_LEVEL_STRINGS_N = sizeof(PRT_LOG_LEVEL_STRINGS)/sizeof(PRT_LOG_LEVEL_STRINGS[0]);

prt::LogLevel getLogLevel() {
	const char* e = std::getenv(PRT_LOG_LEVEL_ENV_VAR);
	if (e == nullptr || strlen(e) == 0)
		return PRT_LOG_LEVEL_DEFAULT;
	for (size_t i = 0; i < PRT_LOG_LEVEL_STRINGS_N; i++)
		if (strcmp(e, PRT_LOG_LEVEL_STRINGS[i]) == 0)
			return static_cast<prt::LogLevel>(i);
	return PRT_LOG_LEVEL_DEFAULT;
}

} // namespace


PRTContext::PRTContext()
        : mLogHandler(new p4h_log::LogHandler(L"p4h")),
          mLicHandle{nullptr},
          mPRTCache{prt::CacheObject::create(prt::CacheObject::CACHE_TYPE_DEFAULT)},
          mRPKUnpackPath{boost::filesystem::temp_directory_path() / ("prt4houdini_" + std::to_string(::getpid()))}
{
    const prt::LogLevel logLevel = getLogLevel();
	prt::setLogLevel(logLevel);
	prt::addLogHandler(mLogHandler.get());

    mCores = std::thread::hardware_concurrency();
    mCores = (mCores == 0) ? 1 : mCores;

    boost::filesystem::path sopPath;
	getLibraryPath(sopPath, reinterpret_cast<const void*>(prt::init));

    prt::FlexLicParams flp;
    const std::string libflexnet = getSharedLibraryPrefix() + FILE_FLEXNET_LIB + getSharedLibrarySuffix();
    const std::string libflexnetPath = (sopPath.parent_path() / libflexnet).string();
    flp.mActLibPath = libflexnetPath.c_str();
    flp.mFeature = "CityEngAdvFx";
    flp.mHostName = "";

    const std::wstring libPath = (sopPath.parent_path() / PRT_LIB_SUBDIR).wstring();
    const wchar_t* extPaths[] = { libPath.c_str() };
    const size_t extPathsCount = sizeof(extPaths)/sizeof(extPaths[0]);

    prt::Status status = prt::STATUS_UNSPECIFIED_ERROR;
    mLicHandle.reset(prt::init(extPaths, extPathsCount, logLevel, &flp, &status));
    if (status != prt::STATUS_OK) {
        LOG_ERR << "Could not get license: " << prt::getStatusDescription(status);
    }
}

PRTContext::~PRTContext() {
    mLicHandle.reset();
	LOG_INF << "PRT shutdown & license returned";

    boost::filesystem::remove_all(mRPKUnpackPath);
    LOG_INF << "Removed RPK unpack directory.";

    prt::removeLogHandler(mLogHandler.get());
}

const ResolveMapUPtr& PRTContext::getResolveMap(const boost::filesystem::path& rpk) {
    if (rpk.empty())
        return mResolveMapNone;

    auto it = mResolveMapCache.find(rpk);
    if (it == mResolveMapCache.end()) {
        prt::Status status = prt::STATUS_UNSPECIFIED_ERROR;
        const auto rpkURI = toFileURI(rpk);
        ResolveMapUPtr rm(prt::createResolveMap(rpkURI.c_str(), mRPKUnpackPath.wstring().c_str(), &status));
        if (status != prt::STATUS_OK)
            return mResolveMapNone;
        it = mResolveMapCache.emplace(rpk, std::move(rm)).first;
    }
    return it->second;
}
