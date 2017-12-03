#pragma once

#include "LogHandler.h"
#include "utils.h"

#include "prt/Object.h"
#include "prt/FlexLicParams.h"

#include "boost/filesystem.hpp"

#include <map>


/**
 * manage PRT "lifetime" (actually, the license lifetime)
 */
struct PRTContext final {
	PRTContext();
	PRTContext(const PRTContext&) = delete;
	PRTContext(PRTContext&&) = delete;
	PRTContext& operator=(PRTContext&) = delete;
	PRTContext& operator=(PRTContext&&) = delete;
	~PRTContext();

	const ResolveMapUPtr& getResolveMap(const boost::filesystem::path& rpk);

	p4h_log::LogHandlerPtr  mLogHandler;
	const prt::Object*      mLicHandle; // TODO: use PRTObjectPtr...
	CacheObjectUPtr         mPRTCache;
	boost::filesystem::path mRPKUnpackPath;
	uint32_t                mCores;

	using ResolveMapCache = std::map<boost::filesystem::path, ResolveMapUPtr>;
	ResolveMapCache         mResolveMapCache;
	const ResolveMapUPtr    mResolveMapNone;
};

using PRTContextUPtr = std::unique_ptr<PRTContext>;
