//
// Created by shaegler on 3/3/18.
//

#include "ResolveMapCache.h"
#include "LogHandler.h"


namespace {

const ResolveMapUPtr RESOLVE_MAP_NONE;

std::chrono::system_clock::time_point getFileModificationTime(const boost::filesystem::path& p) {
	return std::chrono::system_clock::from_time_t(boost::filesystem::last_write_time(p));
}

} // namespace


ResolveMapCache::~ResolveMapCache() {
    boost::filesystem::remove_all(mRPKUnpackPath);
    LOG_INF << "Removed RPK unpack directory";
}

ResolveMapCache::LookupResult ResolveMapCache::get(const boost::filesystem::path& rpk) {
	if (rpk.empty())
		return { RESOLVE_MAP_NONE, CacheStatus::MISS };

	if (!boost::filesystem::exists(rpk))
		return { RESOLVE_MAP_NONE, CacheStatus::MISS };

	const auto timeStamp = getFileModificationTime(rpk);
	LOG_DBG << "rpk: current timestamp: " << std::chrono::duration_cast<std::chrono::nanoseconds>(timeStamp.time_since_epoch()).count() << "ns";

	CacheStatus cs = CacheStatus::HIT;
	auto it = mCache.find(rpk);
	if (it != mCache.end()) {
		LOG_DBG << "rpk: cache timestamp: " << std::chrono::duration_cast<std::chrono::nanoseconds>(it->second.mTimeStamp.time_since_epoch()).count() << "ns";
		if (it->second.mTimeStamp != timeStamp) {
			mCache.erase(it);
			const auto cnt = boost::filesystem::remove_all(mRPKUnpackPath / rpk.leaf());
			LOG_INF << "RPK change detected, forcing reload and clearing cache for " << rpk << " (removed " << cnt << " files)";
			cs = CacheStatus::MISS;
		}
	}
	else
		cs = CacheStatus::MISS;

	if (cs == CacheStatus::MISS) {
		const auto rpkURI = toFileURI(rpk);

		ResolveMapCacheEntry rmce;
		rmce.mTimeStamp = timeStamp;

		prt::Status status = prt::STATUS_UNSPECIFIED_ERROR;
		rmce.mResolveMap.reset(prt::createResolveMap(rpkURI.c_str(), mRPKUnpackPath.wstring().c_str(), &status));
		if (status != prt::STATUS_OK)
			return { RESOLVE_MAP_NONE, CacheStatus::MISS };

		it = mCache.emplace(rpk, std::move(rmce)).first;
		LOG_INF << "Upacked RPK " << rpk << " to " << mRPKUnpackPath;
	}

	return { it->second.mResolveMap, cs };

}

