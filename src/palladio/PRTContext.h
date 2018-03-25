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

#pragma once

#include "PalladioMain.h"
#include "Utils.h"

#include "prt/Object.h"
#include "prt/FlexLicParams.h"

#include "boost/filesystem.hpp"

#include <map>


namespace logging {
class LogHandler;
using LogHandlerPtr = std::unique_ptr<LogHandler>;
}

/**
 * manage PRT "lifetime" (actually, its license lifetime)
 */
struct PLD_TEST_EXPORTS_API PRTContext final {
	explicit PRTContext(const std::vector<boost::filesystem::path>& addExtDirs = {});
	PRTContext(const PRTContext&) = delete;
	PRTContext(PRTContext&&) = delete;
	PRTContext& operator=(PRTContext&) = delete;
	PRTContext& operator=(PRTContext&&) = delete;
	~PRTContext();

	const ResolveMapUPtr& getResolveMap(const boost::filesystem::path& rpk);

	logging::LogHandlerPtr  mLogHandler;
	ObjectUPtr              mLicHandle;
	CacheObjectUPtr         mPRTCache;
	boost::filesystem::path mRPKUnpackPath;
	const uint32_t          mCores;

	using ResolveMapCache = std::map<boost::filesystem::path, ResolveMapUPtr>;
	ResolveMapCache         mResolveMapCache;
	const ResolveMapUPtr    mResolveMapNone;
};

using PRTContextUPtr = std::unique_ptr<PRTContext>;
