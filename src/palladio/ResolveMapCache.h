#pragma once

#include "Utils.h"

#include "BoostRedirect.h"
#include PLD_BOOST_INCLUDE(/filesystem.hpp)

#include <map>
#include <chrono>


class ResolveMapCache {
public:
	using KeyType = std::string;

	explicit ResolveMapCache(const PLD_BOOST_NS::filesystem::path& unpackPath) : mRPKUnpackPath{unpackPath} { }
	ResolveMapCache(const ResolveMapCache&) = delete;
	ResolveMapCache(ResolveMapCache&&) = delete;
	ResolveMapCache& operator=(ResolveMapCache const&) = delete;
	ResolveMapCache& operator=(ResolveMapCache&&) = delete;
	~ResolveMapCache();

	enum class CacheStatus { HIT, MISS };
	using LookupResult = std::pair<const ResolveMapUPtr&, CacheStatus>;
	LookupResult get(const PLD_BOOST_NS::filesystem::path& rpk);

private:
	struct ResolveMapCacheEntry {
		ResolveMapUPtr mResolveMap;
		std::chrono::system_clock::time_point mTimeStamp;
	};
	using Cache = std::map<KeyType, ResolveMapCacheEntry>;
	Cache mCache;

	const PLD_BOOST_NS::filesystem::path mRPKUnpackPath;
};

using ResolveMapCacheUPtr = std::unique_ptr<ResolveMapCache>;