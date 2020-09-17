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

#include "FS/FS_Reader.h"
#include "UT/UT_IStream.h"

namespace {

const ResolveMapSPtr RESOLVE_MAP_NONE;
const ResolveMapCache::LookupResult LOOKUP_FAILURE = {RESOLVE_MAP_NONE, ResolveMapCache::CacheStatus::MISS};
const std::chrono::system_clock::time_point INVALID_TIMESTAMP;

constexpr const char* SCHEMA_OPDEF = "opdef:";
constexpr const char* SCHEMA_OPLIB = "oplib:";
const std::vector<std::string> EMBEDDED_SCHEMAS = {SCHEMA_OPDEF, SCHEMA_OPLIB};

bool isEmbedded(const PLD_BOOST_NS::filesystem::path& p) {
	return startsWithAnyOf(p.string(), EMBEDDED_SCHEMAS);
}

UT_StringHolder getFSReaderFilename(const FS_Reader& fsr) {
#if HOUDINI_VERSION_MAJOR > 16
	return fsr.getFilename();
#else
	UT_String fsrFileName;
	fsr.getFilename(fsrFileName);
	return UT_StringHolder(fsrFileName);
#endif
}

std::chrono::system_clock::time_point getFileModificationTime(const PLD_BOOST_NS::filesystem::path& p) {
	auto actualPath = p;
	if (isEmbedded(p)) {
		FS_Reader fsr(p.string().c_str());
		if (!fsr.isGood())
			return INVALID_TIMESTAMP;
		actualPath = getFSReaderFilename(fsr).toStdString();
	}
	const bool pathExists = PLD_BOOST_NS::filesystem::exists(actualPath);
	const bool isRegularFile = PLD_BOOST_NS::filesystem::is_regular_file(actualPath);
	if (!actualPath.empty() && pathExists && isRegularFile)
		return std::chrono::system_clock::from_time_t(PLD_BOOST_NS::filesystem::last_write_time(actualPath));
	else
		return INVALID_TIMESTAMP;
}

ResolveMapCache::KeyType createCacheKey(const PLD_BOOST_NS::filesystem::path& rpk) {
	return rpk.string(); // TODO: try FS_Reader::splitIndexFileSectionPath for embedded resources
}

struct PathRemover {
	void operator()(PLD_BOOST_NS::filesystem::path const* p) {
		if (p && PLD_BOOST_NS::filesystem::exists(*p)) {
			PLD_BOOST_NS::filesystem::remove(*p);
			LOG_DBG << "Removed file " << *p;
			delete p;
		}
	}
};
using ScopedPath = std::unique_ptr<PLD_BOOST_NS::filesystem::path, PathRemover>;

ScopedPath resolveFromHDA(const PLD_BOOST_NS::filesystem::path& p) {
	LOG_DBG << "detected embedded resource in HDA: " << p;

	FS_Reader fsr(p.string().c_str()); // is able to resolve opdef/oplib URIs
	UT_StringHolder container = getFSReaderFilename(fsr);
	LOG_DBG << "resource container: " << container;

	auto resName = p.leaf().string();
	std::replace(resName.begin(), resName.end(), '?', '_'); // TODO: generalize
	ScopedPath extractedResource(
	        new PLD_BOOST_NS::filesystem::path(PLD_BOOST_NS::filesystem::temp_directory_path() / resName));

	if (fsr.isGood()) {
		UT_WorkBuffer wb;
		fsr.getStream()->getAll(wb);

		std::ofstream out(extractedResource->string(), std::ofstream::binary);
		out.write(wb.buffer(), wb.length());
		out.close();

		LOG_DBG << "Extracted embedded resource into " << *extractedResource;
	}

	return extractedResource;
}

} // namespace

ResolveMapCache::~ResolveMapCache() {
	PLD_BOOST_NS::filesystem::remove_all(mRPKUnpackPath);
	LOG_INF << "Removed RPK unpack directory";
}

ResolveMapCache::LookupResult ResolveMapCache::get(const PLD_BOOST_NS::filesystem::path& rpk) {
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
			const auto cnt = PLD_BOOST_NS::filesystem::remove_all(mRPKUnpackPath / rpk.leaf());
			LOG_INF << "RPK change detected, forcing reload and clearing cache for " << rpk << " (removed " << cnt
			        << " files)";
			cs = CacheStatus::MISS;
		}
	}
	else
		cs = CacheStatus::MISS;

	if (cs == CacheStatus::MISS) {
		ScopedPath extractedPath; // if set, will resolve the extracted RPK from HDA
		const auto actualRPK = [&extractedPath](const PLD_BOOST_NS::filesystem::path& p) {
			if (isEmbedded(p)) {
				extractedPath = resolveFromHDA(p);
				return *extractedPath;
			}
			else
				return p;
		}(rpk);

		const auto rpkURI = toFileURI(actualRPK);

		ResolveMapCacheEntry rmce;
		rmce.mTimeStamp = timeStamp;

		prt::Status status = prt::STATUS_UNSPECIFIED_ERROR;
		LOG_DBG << "createResolveMap from " << rpkURI;
		rmce.mResolveMap.reset(prt::createResolveMap(rpkURI.c_str(), mRPKUnpackPath.wstring().c_str(), &status),
		                       PRTDestroyer());
		if (status != prt::STATUS_OK)
			return LOOKUP_FAILURE;

		it = mCache.emplace(cacheKey, std::move(rmce)).first;
		LOG_INF << "Upacked RPK " << actualRPK << " to " << mRPKUnpackPath;
	}

	return {it->second.mResolveMap, cs};
}
