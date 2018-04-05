#pragma once

#include "Utils.h"

#include "boost/filesystem.hpp"

#include <map>
#include <chrono>


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
		std::chrono::system_clock::time_point mTimeStamp;
	};
	using Cache = std::map<boost::filesystem::path, ResolveMapCacheEntry>;
	Cache mCache;

	const boost::filesystem::path mRPKUnpackPath;
};

using ResolveMapCacheUPtr = std::unique_ptr<ResolveMapCache>;