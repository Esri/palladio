/*
 * Copyright 2014-2018 Esri R&D Zurich and VRBN
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

#include "PRTContext.h"
#include "PalladioMain.h"
#include "LogHandler.h"
#include "SOPAssign.h"

#include "OP/OP_Node.h"
#include "OP/OP_Director.h"
#include "OP/OP_Network.h"

#include <thread>
#include <process.h>


namespace {

constexpr const wchar_t*      PLD_LOG_PREFIX          = L"pld";
constexpr const char*         PLD_TMP_PREFIX          = "palladio_";

constexpr const char*         PRT_LIB_SUBDIR          = "prtlib";
constexpr const char*         FILE_FLEXNET_LIB        = "flexnet_prt";

constexpr const char*         PRT_LIC_ENV_VAR         = "CITYENGINE_LICENSE_SERVER";

constexpr const prt::LogLevel PRT_LOG_LEVEL_DEFAULT   = prt::LOG_ERROR;
constexpr const char*         PRT_LOG_LEVEL_ENV_VAR   = "CITYENGINE_LOG_LEVEL";
constexpr const char*         PRT_LOG_LEVEL_STRINGS[] = { "trace", "debug", "info", "warning", "error", "fatal" };
constexpr const size_t        PRT_LOG_LEVEL_STRINGS_N = sizeof(PRT_LOG_LEVEL_STRINGS)/sizeof(PRT_LOG_LEVEL_STRINGS[0]);

class License {
private:
    prt::FlexLicParams flexLicParams;

	std::string libflexnetPath; // owns flexLicParams.mActLibPath char ptr
	std::string licFeature;     // owns flexLicParams.mFeature
	std::string licServer;      // owns flexLicParams.mHostName

public:
	License(const boost::filesystem::path& prtRootPath) {
        const std::string libflexnet = getSharedLibraryPrefix() + FILE_FLEXNET_LIB + getSharedLibrarySuffix();

		libflexnetPath = (prtRootPath / libflexnet).string();
		flexLicParams.mActLibPath = libflexnetPath.c_str();

		const char* e = std::getenv(PRT_LIC_ENV_VAR);
		if (e == nullptr || strlen(e) == 0)
			licFeature = "CityEngAdvFx"; // empty license server string: assuming node-locked license
		else {
			licFeature = "CityEngAdv"; // floating/network license
			licServer.assign(e);
		}

		flexLicParams.mFeature = licFeature.c_str();
        flexLicParams.mHostName = licServer.c_str();

		LOG_INF << "CityEngine license: feature = '" << licFeature << "', server = '" << licServer << "'";
	}

	const prt::LicParams* getParams() const {
		return &flexLicParams;
	}
};

// TODO: move to LogHandler
prt::LogLevel getLogLevel() {
	const char* e = std::getenv(PRT_LOG_LEVEL_ENV_VAR);
	if (e == nullptr || strlen(e) == 0)
		return PRT_LOG_LEVEL_DEFAULT;
	for (size_t i = 0; i < PRT_LOG_LEVEL_STRINGS_N; i++)
		if (strcmp(e, PRT_LOG_LEVEL_STRINGS[i]) == 0)
			return static_cast<prt::LogLevel>(i);
	return PRT_LOG_LEVEL_DEFAULT;
}

template<typename C>
std::vector<const C*> toPtrVec(const std::vector<std::basic_string<C>>& sv) {
	std::vector<const C*> pv(sv.size());
	std::transform(sv.begin(), sv.end(), pv.begin(), [](const std::basic_string<C>& s){ return s.c_str(); });
	return pv;
}

uint32_t getNumCores() {
	const auto n = std::thread::hardware_concurrency();
	return n > 0 ? n : 1;
}

/**
 * schedule recook of all assign nodes with matching rpk
 */
void scheduleRecook(const boost::filesystem::path& rpk) {
	auto visit = [](OP_Node& n, void* data) -> bool {
		if (n.getOperator()->getName().equal(OP_PLD_ASSIGN)) {
			auto rpk = reinterpret_cast<boost::filesystem::path*>(data);
			SOPAssign& sa = static_cast<SOPAssign&>(n);
			if (sa.getRPK() == *rpk) {
				LOG_DBG << "forcing recook of: " << n.getName() << ", " << n.getOpType() << ", " << n.getOperator()->getName();
				sa.forceRecook();
			}
		}
		return false;
	};

	OP_Network* objMgr = OPgetDirector()->getManager("obj");
	objMgr->traverseChildren(visit, const_cast<void*>(reinterpret_cast<const void*>(&rpk)), true);
}

} // namespace


PRTContext::PRTContext(const std::vector<boost::filesystem::path>& addExtDirs)
        : mLogHandler(new logging::LogHandler(PLD_LOG_PREFIX)),
          mLicHandle{nullptr},
          mPRTCache{prt::CacheObject::create(prt::CacheObject::CACHE_TYPE_DEFAULT)},
          mCores{getNumCores()},
          mResolveMapCache{new ResolveMapCache(boost::filesystem::temp_directory_path() / (PLD_TMP_PREFIX + std::to_string(::getpid())))}
{
    const prt::LogLevel logLevel = getLogLevel();
	prt::setLogLevel(logLevel);
	prt::addLogHandler(mLogHandler.get());

	// -- get the dir containing prt core library
	const auto rootPath = [](){
        boost::filesystem::path prtCorePath;
		getLibraryPath(prtCorePath, reinterpret_cast<const void*>(prt::init));
		return prtCorePath.parent_path();
	}();

	// -- detect license
	const License license(rootPath);

	// -- scan for directories with prt extensions
	const std::vector<boost::filesystem::path> extDirs = [&rootPath,&addExtDirs](){
		std::vector<boost::filesystem::path> ed;
		ed.emplace_back(rootPath / PRT_LIB_SUBDIR);
		for (auto d: addExtDirs) { // get a copy
			if (boost::filesystem::is_regular_file(d))
				d = d.parent_path();
			if (!d.is_absolute())
				d = rootPath / d;
			ed.emplace_back(d);
		}
		return ed;
	}();
	const std::vector<std::wstring> extDirStrs = [&extDirs]() {
		std::vector<std::wstring> sv;
		for (const auto& d: extDirs)
			sv.emplace_back(d.wstring());
		return sv;
	}();
	const auto extDirCStrs = toPtrVec(extDirStrs); // depends on extDirStrs life-time!

	// -- initialize PRT
    prt::Status status = prt::STATUS_UNSPECIFIED_ERROR;
    mLicHandle.reset(prt::init(extDirCStrs.data(), extDirCStrs.size(), logLevel, license.getParams(), &status));
    if (status != prt::STATUS_OK) {
        LOG_ERR << "Could not get license: " << prt::getStatusDescription(status);
    }
}

PRTContext::~PRTContext() {
    mResolveMapCache.reset();
	LOG_INF << "Released RPK Cache";

	mPRTCache.reset(); // calling reset manually to ensure order
	LOG_INF << "Released PRT cache";

	mLicHandle.reset(); // same here
	LOG_INF << "Shutdown PRT & returned license";

    prt::removeLogHandler(mLogHandler.get());
}

const ResolveMapUPtr& PRTContext::getResolveMap(const boost::filesystem::path& rpk) {
	auto lookupResult = mResolveMapCache->get(rpk);
	if (lookupResult.second == ResolveMapCache::CacheStatus::MISS) {
		mPRTCache->flushAll();
		scheduleRecook(rpk);
	}
	return lookupResult.first;
}
