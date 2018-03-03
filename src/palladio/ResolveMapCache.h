#pragma once

#include "Utils.h"

#include "boost/filesystem.hpp"

#include <sys/types.h>
#include <sys/stat.h>
#ifndef WIN32
#   include <unistd.h>
#endif


class ResolveMapCache {
public:
	ResolveMapCache(const boost::filesystem::path& unpackPath) : mRPKUnpackPath{unpackPath} { }
	ResolveMapCache(const ResolveMapCache&) = delete;
	ResolveMapCache(ResolveMapCache&&) = delete;
	ResolveMapCache& operator=(ResolveMapCache&) = delete;
	~ResolveMapCache();

	enum class CacheStatus { HIT, MISS };
	using LookupResult = std::pair<const ResolveMapUPtr&, CacheStatus>;
	LookupResult get(const boost::filesystem::path& rpk);

private:
	struct ResolveMapCacheEntry {
		ResolveMapUPtr mResolveMap;
		timespec mTimeStamp;
	};
	using Cache = std::map<boost::filesystem::path, ResolveMapCacheEntry>;
	Cache mCache;

	const boost::filesystem::path mRPKUnpackPath;
};

using ResolveMapCacheUPtr = std::unique_ptr<ResolveMapCache>;