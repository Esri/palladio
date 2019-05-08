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

#include <string>
#include <map>
#include <chrono>
#include <vector>


#define WA(x) //WatchAgent wa_(x, __PRETTY_FUNCTION__)
#define WA_NEW_LAP //theWatch.newLap();
#define WA_PRINT_TIMINGS //theWatch.printTimings();


class MultiWatch {
public:
	using Lap = std::map<std::string, double>;
	std::vector<Lap> laps;
	std::map<std::string, std::chrono::time_point<std::chrono::system_clock>> starts;

	MultiWatch();
	void printTimings() const;
	void start(const std::string& name);
	void stop(const std::string& name);

	void newLap();
};

extern MultiWatch theWatch;


class WatchAgent {
public:
	const std::string mContext;
	const std::string mName;

	WatchAgent(const std::string& name, const std::string& ctx);
	~WatchAgent();

private:
	static std::string stripArgs(const std::string& s);
};
