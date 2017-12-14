#pragma once

#include "utils.h"

#include "prt/Object.h"
#include "prt/FlexLicParams.h"

#include "boost/filesystem.hpp"

#include <map>


namespace p4h_log {
class LogHandler;
using LogHandlerPtr = std::unique_ptr<LogHandler>;
}

/**
 * manage PRT "lifetime" (actually, the license lifetime)
 */
struct PRTContext final {
	explicit PRTContext(const std::vector<boost::filesystem::path>& addExtDirs = {});
	PRTContext(const PRTContext&) = delete;
	PRTContext(PRTContext&&) = delete;
	PRTContext& operator=(PRTContext&) = delete;
	PRTContext& operator=(PRTContext&&) = delete;
	~PRTContext();

	const ResolveMapUPtr& getResolveMap(const boost::filesystem::path& rpk);

	p4h_log::LogHandlerPtr  mLogHandler;
	ObjectUPtr              mLicHandle;
	CacheObjectUPtr         mPRTCache;
	boost::filesystem::path mRPKUnpackPath;
	uint32_t                mCores;

	using ResolveMapCache = std::map<boost::filesystem::path, ResolveMapUPtr>;
	ResolveMapCache         mResolveMapCache;
	const ResolveMapUPtr    mResolveMapNone;
};

using PRTContextUPtr = std::unique_ptr<PRTContext>;
