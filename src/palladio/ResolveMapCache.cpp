/*
 * Copyright 2014-2020 Esri R&D Zurich and VRBN
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "ResolveMapCache.h"
#include "LogHandler.h"

#ifndef PLD_TEST_EXPORTS
#	include "FS/FS_Reader.h"
#	include "UT/UT_IStream.h"
#endif

namespace {

constexpr bool UNPACK_RULE_PACKAGES = false;

const ResolveMapSPtr RESOLVE_MAP_NONE;
const ResolveMapCache::LookupResult LOOKUP_FAILURE = {RESOLVE_MAP_NONE, ResolveMapCache::CacheStatus::MISS};
const std::filesystem::file_time_type INVALID_TIMESTAMP;

constexpr const char* SCHEMA_OPDEF = "opdef:";
constexpr const char* SCHEMA_OPLIB = "oplib:";
const std::vector<std::string> EMBEDDED_SCHEMAS = {SCHEMA_OPDEF, SCHEMA_OPLIB};

bool isEmbedded(const std::filesystem::path& p) {
	return startsWithAnyOf(p.string(), EMBEDDED_SCHEMAS);
}

#ifndef PLD_TEST_EXPORTS
UT_StringHolder getFSReaderFilename(const FS_Reader& fsr) {
	return fsr.getFilename();
}
#endif

std::filesystem::file_time_type getFileModificationTime(const std::filesystem::path& p) {
	auto actualPath = p;
	if (isEmbedded(p)) {
#ifndef PLD_TEST_EXPORTS
		FS_Reader fsr(p.string().c_str());
		if (!fsr.isGood())
			return INVALID_TIMESTAMP;
		actualPath = getFSReaderFilename(fsr).toStdString();
#endif
	}
	const bool pathExists = std::filesystem::exists(actualPath);
	const bool isRegularFile = std::filesystem::is_regular_file(actualPath);
	if (!actualPath.empty() && pathExists && isRegularFile)
		return std::filesystem::last_write_time(actualPath);
	else
		return INVALID_TIMESTAMP;
}

ResolveMapCache::KeyType createCacheKey(const std::filesystem::path& rpk) {
	return rpk.string(); // TODO: try FS_Reader::splitIndexFileSectionPath for embedded resources
}

#ifndef PLD_TEST_EXPORTS
std::filesystem::path resolveFromHDA(const std::filesystem::path& p, const std::filesystem::path& unpackPath) {
	LOG_DBG << "detected embedded resource in HDA: " << p;

	FS_Reader fsr(p.string().c_str()); // is able to resolve opdef/oplib URIs
	UT_StringHolder container = getFSReaderFilename(fsr);
	LOG_DBG << "resource container: " << container;

	auto resName = p.filename().string();
	std::replace(resName.begin(), resName.end(), '?', '_'); // TODO: generalize
	std::filesystem::path extractedResource = unpackPath / resName;

	if (fsr.isGood()) {
		UT_WorkBuffer wb;
		fsr.getStream()->getAll(wb);

		std::ofstream out(extractedResource.string(), std::ofstream::binary);
		out.write(wb.buffer(), wb.length());
		out.close();

		LOG_DBG << "Extracted embedded resource into " << extractedResource;
	}
	return extractedResource;
}
#endif

} // namespace

ResolveMapCache::~ResolveMapCache() {
	std::filesystem::remove_all(mRPKUnpackPath);
	LOG_INF << "Removed RPK unpack directory";
}

ResolveMapCache::LookupResult ResolveMapCache::get(const std::filesystem::path& rpk) {
	const auto cacheKey = createCacheKey(rpk);

	const auto timeStamp = getFileModificationTime(rpk);
	LOG_DBG << "rpk: current timestamp: "
	        << std::chrono::duration_cast<std::chrono::nanoseconds>(timeStamp.time_since_epoch()).count() << "ns";

	// verify timestamp
	if (timeStamp == INVALID_TIMESTAMP)
		return LOOKUP_FAILURE;

	CacheStatus cs = CacheStatus::HIT;
	auto it = mCache.find(cacheKey);
	if (it != mCache.end()) {
		LOG_DBG << "rpk: cache timestamp: "
		        << std::chrono::duration_cast<std::chrono::nanoseconds>(it->second.mTimeStamp.time_since_epoch())
		                   .count()
		        << "ns";
		if (it->second.mTimeStamp != timeStamp) {
			mCache.erase(it);
			const auto cnt = std::filesystem::remove_all(mRPKUnpackPath / rpk.filename());
			LOG_INF << "RPK change detected, forcing reload and clearing cache for " << rpk << " (removed " << cnt
			        << " files)";
			cs = CacheStatus::MISS;
		}
	}
	else
		cs = CacheStatus::MISS;

	if (cs == CacheStatus::MISS) {
		std::filesystem::path extractedPath; // if set, will resolve the extracted RPK from HDA
		const auto actualRPK = [&extractedPath](const std::filesystem::path& p,
		                                        const std::filesystem::path& mRPKUnpackPath) {
			if (isEmbedded(p)) {
#ifndef PLD_TEST_EXPORTS
				extractedPath = resolveFromHDA(p, mRPKUnpackPath);
#endif
				return extractedPath;
			}
			else
				return p;
		}(rpk, mRPKUnpackPath);

		const auto rpkURI = toFileURI(actualRPK);

		ResolveMapCacheEntry rmce;
		rmce.mTimeStamp = timeStamp;

		prt::Status status = prt::STATUS_UNSPECIFIED_ERROR;
		LOG_DBG << "createResolveMap from " << rpkURI;
		const wchar_t* extractionPathPtr = UNPACK_RULE_PACKAGES ? mRPKUnpackPath.wstring().c_str() : nullptr;
		rmce.mResolveMap.reset(prt::createResolveMap(rpkURI.c_str(), extractionPathPtr, &status), PRTDestroyer());
		if (status != prt::STATUS_OK)
			return LOOKUP_FAILURE;

		it = mCache.emplace(cacheKey, std::move(rmce)).first;
		LOG_INF << "Upacked RPK " << actualRPK << " to " << mRPKUnpackPath;
	}

	return {it->second.mResolveMap, cs};
}
