/*
 * Copyright 2014-2019 Esri R&D Zurich and VRBN
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

#pragma once

#include "PalladioMain.h"
#include "ResolveMapCache.h"
#include "Utils.h"

#include "prt/Object.h"

#include "BoostRedirect.h"
#include PLD_BOOST_INCLUDE(/filesystem.hpp)

#include <map>


namespace logging {
class LogHandler;
using LogHandlerPtr = std::unique_ptr<LogHandler>;
}

/**
 * manage PRT "lifetime" (actually, its license lifetime)
 */
struct PLD_TEST_EXPORTS_API PRTContext final {
	explicit PRTContext(const std::vector<PLD_BOOST_NS::filesystem::path>& addExtDirs = {});
	PRTContext(const PRTContext&) = delete;
	PRTContext(PRTContext&&) = delete;
	PRTContext& operator=(PRTContext&) = delete;
	PRTContext& operator=(PRTContext&&) = delete;
	~PRTContext();

	ResolveMapSPtr getResolveMap(const PLD_BOOST_NS::filesystem::path& rpk);
	bool isAlive() const { return mPRTHandle.operator bool(); }

	logging::LogHandlerPtr  mLogHandler;
	ObjectUPtr              mPRTHandle;
	CacheObjectUPtr         mPRTCache;
	const uint32_t          mCores;
	ResolveMapCacheUPtr     mResolveMapCache;
};

using PRTContextUPtr = std::unique_ptr<PRTContext>;
