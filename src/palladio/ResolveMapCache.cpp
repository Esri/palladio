//
// Created by shaegler on 3/3/18.
//

#include "ResolveMapCache.h"
#include "LogHandler.h"


namespace {

const ResolveMapUPtr RESOLVE_MAP_NONE;
const timespec       NO_MODIFICATION_TIME{0, 0};

timespec getFileModificationTime(const boost::filesystem::path& p) {
	struct stat result;
	if (stat(p.c_str(), &result) == 0) {
		return result.st_mtim;
	}
	else
		return NO_MODIFICATION_TIME;
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
	LOG_DBG << "rpk: current timestamp: " << timeStamp.tv_sec << "s " << timeStamp.tv_nsec << "ns";

	CacheStatus cs = CacheStatus::HIT;
	auto it = mCache.find(rpk);
	if (it != mCache.end()) {
		LOG_DBG << "rpk: cache timestamp: " << it->second.mTimeStamp.tv_sec << "s " << it->second.mTimeStamp.tv_nsec << "ns";
		if (it->second.mTimeStamp.tv_sec != timeStamp.tv_sec || it->second.mTimeStamp.tv_nsec != timeStamp.tv_nsec) {
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

