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

#include "prt/API.h"
#include "prt/LogHandler.h"

#include <iostream>
#include <iterator>
#include <memory>
#include <ostream>
#include <sstream>
#include <string>
#include <vector>

namespace logging {

prt::LogLevel getDefaultLogLevel();

class ScopedLogLevelModifier {
public:
	ScopedLogLevelModifier() = delete;
	explicit ScopedLogLevelModifier(prt::LogLevel newLevel) : mOriginalLevel(prt::getLogLevel()) {
		prt::setLogLevel(newLevel);
	}
	~ScopedLogLevelModifier() {
		prt::setLogLevel(mOriginalLevel);
	}

private:
	prt::LogLevel mOriginalLevel;
};

struct Logger {};

const std::string LEVELS[] = {"trace", "debug", "info", "warning", "error", "fatal"};
const std::wstring WLEVELS[] = {L"trace", L"debug", L"info", L"warning", L"error", L"fatal"};

// log to std streams
template <prt::LogLevel L>
struct StreamLogger : Logger {
	StreamLogger(std::wostream& out = std::wcout) : Logger(), mOut(out) {
		mOut << prefix();
	}
	virtual ~StreamLogger() {
		mOut << std::endl;
	}
	StreamLogger<L>& operator<<(std::wostream& (*x)(std::wostream&)) {
		mOut << x;
		return *this;
	}
	StreamLogger<L>& operator<<(const std::string& x) {
		std::copy(x.begin(), x.end(), std::ostream_iterator<char, wchar_t>(mOut));
		return *this;
	}
	template <typename T>
	StreamLogger<L>& operator<<(const T& x) {
		mOut << x;
		return *this;
	}
	static std::wstring prefix() {
		return L"[" + WLEVELS[L] + L"] ";
	}
	std::wostream& mOut;
};

// log through the prt logger
template <prt::LogLevel L>
struct PRTLogger : Logger {
	PRTLogger() : Logger() {}
	virtual ~PRTLogger() {
		prt::log(wstr.str().c_str(), L);
	}
	PRTLogger<L>& operator<<(std::wostream& (*x)(std::wostream&)) {
		wstr << x;
		return *this;
	}
	PRTLogger<L>& operator<<(const std::string& x) {
		std::copy(x.begin(), x.end(), std::ostream_iterator<char, wchar_t>(wstr));
		return *this;
	}
	template <typename T>
	PRTLogger<L>& operator<<(const std::vector<T>& v) {
		wstr << L"[ ";
		for (const T& x : v) {
			wstr << x << L" ";
		}
		wstr << L"]";
		return *this;
	}
	template <typename T>
	PRTLogger<L>& operator<<(const T& x) {
		wstr << x;
		return *this;
	}
	std::wostringstream wstr;
};

class LogHandler : public prt::LogHandler {
public:
	LogHandler(const std::wstring& name) : mName(name) {}

	virtual void handleLogEvent(const wchar_t* msg, prt::LogLevel level) override {
		// probably not the best idea - is there a houdini logging framework?
		std::wcout << L"[" << mName << L"] " << msg << std::endl;
	}

	virtual const prt::LogLevel* getLevels(size_t* count) override {
		*count = prt::LogHandler::ALL_COUNT;
		return prt::LogHandler::ALL;
	}

	virtual void getFormat(bool* dateTime, bool* level) override {
		*dateTime = true;
		*level = true;
	}

	void setName(const std::wstring& n) {
		mName = n;
	}

private:
	std::wstring mName;
};

using LogHandlerPtr = std::unique_ptr<LogHandler>;

// switch logger here between PRTLogger and StreamLogger
template <prt::LogLevel L>
using LT = PRTLogger<L>;

} // namespace logging

using _LOG_DBG = logging::LT<prt::LOG_DEBUG>;
using _LOG_INF = logging::LT<prt::LOG_INFO>;
using _LOG_WRN = logging::LT<prt::LOG_WARNING>;
using _LOG_ERR = logging::LT<prt::LOG_ERROR>;
using _LOG_FTL = logging::LT<prt::LOG_FATAL>;

// convenience shortcuts in global namespace
#define LOG_DBG _LOG_DBG() << __FUNCTION__ << ": "
#define LOG_INF _LOG_INF()
#define LOG_WRN _LOG_WRN()
#define LOG_ERR _LOG_ERR()
#define LOG_FTL _LOG_FTL()
