#pragma once

#include "client/logging.h"
#include "client/utils.h"

#include "prt/Object.h"
#include "prt/FlexLicParams.h"

#include "boost/filesystem.hpp"

#include <thread>
#include <map>


namespace {

// global prt settings
const prt::LogLevel	PRT_LOG_LEVEL		= prt::LOG_DEBUG;
const char*			PRT_LIB_SUBDIR		= "prtlib";
const char*			FILE_FLEXNET_LIB	= "flexnet_prt";

} // namespace


namespace p4h {

/**
 * manage PRT "lifetime" (actually, the license lifetime)
 */
struct PRTContext final {
	PRTContext()
	: mLogHandler(new log::LogHandler(L"p4h global", prt::LOG_ERROR))
	, mLicHandle{nullptr}
	, mPRTCache{prt::CacheObject::create(prt::CacheObject::CACHE_TYPE_DEFAULT)}
	, mRPKUnpackPath{boost::filesystem::temp_directory_path() / "prt4houdini"}
	{
		prt::addLogHandler(mLogHandler.get());

		mCores = std::thread::hardware_concurrency();
		mCores = (mCores == 0) ? 1 : mCores;

		boost::filesystem::path sopPath;
		p4h::utils::getPathToCurrentModule(sopPath);

		prt::FlexLicParams flp;
		std::string libflexnet = p4h::utils::getSharedLibraryPrefix() + FILE_FLEXNET_LIB + p4h::utils::getSharedLibrarySuffix();
		std::string libflexnetPath = (sopPath.parent_path() / libflexnet).string();
		flp.mActLibPath = libflexnetPath.c_str();
		flp.mFeature = "CityEngAdvFx";
		flp.mHostName = "";

		std::wstring libPath = (sopPath.parent_path() / PRT_LIB_SUBDIR).wstring();
		const wchar_t* extPaths[] = { libPath.c_str() };

		prt::Status status = prt::STATUS_UNSPECIFIED_ERROR;
		mLicHandle = prt::init(extPaths, 1, PRT_LOG_LEVEL, &flp, &status); // TODO: add UI for log level control
		if (status != prt::STATUS_OK) {
			LOG_ERR << "Could not get license: " << prt::getStatusDescription(status);
		}
	}

	PRTContext(PRTContext&) = delete;
	PRTContext& operator=(PRTContext&) = delete;

	virtual ~PRTContext() {
		if (mLicHandle) {
			mLicHandle->destroy();
			LOG_INF << "PRT license destroyed.";
		}

		boost::filesystem::remove_all(mRPKUnpackPath);
		LOG_INF << "Removed RPK unpack directory.";

		prt::removeLogHandler(mLogHandler.get());
	}

	// TODO: make thread-safe
	const ResolveMapUPtr& getResolveMap(const std::wstring& rpk) {
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

	log::LogHandlerPtr      mLogHandler;
	const prt::Object*      mLicHandle; // TODO: use PRTObjectPtr...
	CacheObjectPtr          mPRTCache;
	boost::filesystem::path mRPKUnpackPath;
	int8_t                  mCores;

	using ResolveMapCache = std::map<std::wstring, ResolveMapUPtr>;
	ResolveMapCache         mResolveMapCache;
	const ResolveMapUPtr	mResolveMapNone;
};

using PRTContextUPtr = std::unique_ptr<PRTContext>;

} // namespace p4h
