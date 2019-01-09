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

#include "MultiWatch.h"
#include "LogHandler.h"

#include <mutex>
#include <iomanip>
#include <algorithm>
#include <numeric>
#include <cmath>


namespace {

std::mutex watchMutex;

} // namespace

MultiWatch theWatch;

MultiWatch::MultiWatch() {
	newLap();
}

void MultiWatch::newLap() {
	std::lock_guard<std::mutex> guard(watchMutex);
	laps.emplace_back(Lap());
}

void MultiWatch::start(const std::string& name) {
	const auto n = std::chrono::system_clock::now();
	{
		std::lock_guard<std::mutex> guard(watchMutex);
		starts[name] = n;
	}
}

void MultiWatch::stop(const std::string& name) {
	const auto n = std::chrono::system_clock::now();
	{
		std::lock_guard<std::mutex> guard(watchMutex);
		const auto s = starts.at(name);
		const auto d = std::chrono::duration<double>(n - s).count();

		Lap& l = laps.back();
		auto it = l.emplace(name, d);
		if (!it.second)
			it.first->second += d;
	}
}

void MultiWatch::printTimings() const {
	std::map<std::string, std::vector<double>> lapTimes;
	for (const auto& l: laps) {
		for (const auto& ctx: l) {
			lapTimes[ctx.first].push_back(ctx.second);
		}
	}

	LOG_FTL << "-- timings:";
	for (const auto& l: lapTimes) {
		const auto& ctxName = l.first;
		const auto& lt = l.second;

		const size_t numLaps = lapTimes.size();
		const double lapSum = std::accumulate(lt.begin(), lt.end(), 0.0);
		const double lapAvg = lapSum / (double)numLaps;
		const auto avgIntStrWidth = std::to_string(std::trunc(lapAvg)).size();
		LOG_FTL << "   " << ctxName
		        << std::setw(75 - ctxName.size()) << std::right << std::fixed << std::setprecision(5-avgIntStrWidth) << lapAvg << "s"
		        << std::setw(15) << std::right << "(#laps = " << numLaps << ")";
	}
}

WatchAgent::WatchAgent(const std::string& name, const std::string& ctx) : mName(name), mContext(stripArgs(ctx)) {
	theWatch.start(mContext + ": " + mName);
}

WatchAgent::~WatchAgent() {
	theWatch.stop(mContext + ": " + mName);
}

std::string WatchAgent::stripArgs(const std::string& s) {
	std::string t = s.substr(0, s.find_first_of('('));
	auto p = t.find_last_of(' ');
	return (p != std::string::npos) ? t.substr(p+1) : t;
}
