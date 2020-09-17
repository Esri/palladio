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

#include "LogHandler.h"

#include <cstring>

namespace {

constexpr const prt::LogLevel PRT_LOG_LEVEL_DEFAULT = prt::LOG_ERROR;
constexpr const char* PRT_LOG_LEVEL_ENV_VAR = "CITYENGINE_LOG_LEVEL";
constexpr const char* PRT_LOG_LEVEL_STRINGS[] = {"trace", "debug", "info", "warning", "error", "fatal"};
constexpr const size_t PRT_LOG_LEVEL_STRINGS_N = sizeof(PRT_LOG_LEVEL_STRINGS) / sizeof(PRT_LOG_LEVEL_STRINGS[0]);

} // namespace

namespace logging {

prt::LogLevel getDefaultLogLevel() {
	const char* e = std::getenv(PRT_LOG_LEVEL_ENV_VAR);
	if (e == nullptr || std::strlen(e) == 0)
		return PRT_LOG_LEVEL_DEFAULT;
	for (size_t i = 0; i < PRT_LOG_LEVEL_STRINGS_N; i++)
		if (std::strcmp(e, PRT_LOG_LEVEL_STRINGS[i]) == 0)
			return static_cast<prt::LogLevel>(i);
	return PRT_LOG_LEVEL_DEFAULT;
}

} // namespace logging