#include "PRTContext.h"

#include <thread>


namespace {

// global prt settings
constexpr const prt::LogLevel PRT_INIT_LOG_LEVEL = prt::LOG_INFO; // TODO: connect to HOUDINI_DSO_ERROR env var
constexpr const char*         PRT_LIB_SUBDIR     = "prtlib";
constexpr const char*         FILE_FLEXNET_LIB   = "flexnet_prt";

} // namespace


namespace p4h {

PRTContext::PRTContext()
        : mLogHandler(new log::LogHandler(L"p4h", PRT_INIT_LOG_LEVEL)), mLicHandle{nullptr},
          mPRTCache{prt::CacheObject::create(prt::CacheObject::CACHE_TYPE_DEFAULT)},
          mRPKUnpackPath{boost::filesystem::temp_directory_path() / "prt4houdini"}
{
    prt::addLogHandler(mLogHandler.get());

    mCores = std::thread::hardware_concurrency();
    mCores = (mCores == 0) ? 1 : mCores;

    boost::filesystem::path sopPath;
    p4h::utils::getPathToCurrentModule(sopPath);

    prt::FlexLicParams flp;
    const std::string libflexnet = p4h::utils::getSharedLibraryPrefix() + FILE_FLEXNET_LIB + p4h::utils::getSharedLibrarySuffix();
    const std::string libflexnetPath = (sopPath.parent_path() / libflexnet).string();
    flp.mActLibPath = libflexnetPath.c_str();
    flp.mFeature = "CityEngAdvFx";
    flp.mHostName = "";

    const std::wstring libPath = (sopPath.parent_path() / PRT_LIB_SUBDIR).wstring();
    const wchar_t* extPaths[] = { libPath.c_str() };
    const size_t extPathsCount = sizeof(extPaths)/sizeof(extPaths[0]);

    prt::Status status = prt::STATUS_UNSPECIFIED_ERROR;
    mLicHandle = prt::init(extPaths, extPathsCount, PRT_INIT_LOG_LEVEL, &flp, &status);
    if (status != prt::STATUS_OK) {
        LOG_ERR << "Could not get license: " << prt::getStatusDescription(status);
    }
}

PRTContext::~PRTContext() {
    if (mLicHandle) {
        mLicHandle->destroy();
        LOG_INF << "PRT license destroyed.";
    }

    boost::filesystem::remove_all(mRPKUnpackPath);
    LOG_INF << "Removed RPK unpack directory.";

    prt::removeLogHandler(mLogHandler.get());
}

// TODO: make thread-safe to support multiple assign nodes
const ResolveMapUPtr &PRTContext::getResolveMap(const std::wstring &rpk) {
    auto it = mResolveMapCache.find(rpk);
    if (it == mResolveMapCache.end()) {
        prt::Status status = prt::STATUS_UNSPECIFIED_ERROR;
        ResolveMapUPtr rm(prt::createResolveMap(rpk.c_str(), mRPKUnpackPath.wstring().c_str(), &status));
        if (status != prt::STATUS_OK)
            return mResolveMapNone;
        it = mResolveMapCache.emplace(rpk, std::move(rm)).first;
    }
    return it->second;
}

} // namespace p4h