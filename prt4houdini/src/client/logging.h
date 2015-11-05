#pragma once

#include "prt/API.h"
#include "prt/LogHandler.h"

#include <string>
#include <vector>
#include <iterator>
#include <ostream>
#include <sstream>
#include <iostream>


namespace p4h {
namespace log {

struct Logger { // TODO: use std::ostream as basis
};

const std::string LEVELS[] = { "trace", "debug", "info", "warning", "error", "fatal" };
const std::wstring WLEVELS[] = { L"trace", L"debug", L"info", L"warning", L"error", L"fatal" };

// log to std streams
template<prt::LogLevel L> struct StreamLogger : public Logger {
	StreamLogger(std::wostream& out = std::wcout) : Logger(), mOut(out) { mOut << prefix(); }
	virtual ~StreamLogger() { mOut << std::endl; }
	StreamLogger<L>& operator<<(std::wostream&(*x)(std::wostream&)) { mOut << x; return *this; }
	StreamLogger<L>& operator<<(const std::string& x) { std::copy(x.begin(), x.end(), std::ostream_iterator<char, wchar_t>(mOut)); return *this; }
	template<typename T> StreamLogger<L>& operator<<(const T& x) { mOut << x; return *this; }
	static std::wstring prefix() { return L"[" + WLEVELS[L] + L"] "; }
	std::wostream& mOut;
};

// log through the prt logger
template<prt::LogLevel L> struct PRTLogger : public Logger {
	PRTLogger() : Logger() { }
	virtual ~PRTLogger() { prt::log(wstr.str().c_str(), L); }
	PRTLogger<L>& operator<<(std::wostream&(*x)(std::wostream&)) { wstr << x;  return *this; }
	PRTLogger<L>& operator<<(const std::string& x) {
		std::copy(x.begin(), x.end(), std::ostream_iterator<char, wchar_t>(wstr));
		return *this;
	}
	template<typename T> PRTLogger<L>& operator<<(const std::vector<T>& v) {
		wstr << L"[ ";
		for (const T& x: v) {
			wstr << x << L" ";
		}
		wstr << L"]";
		return *this;
	}
	template<typename T> PRTLogger<L>& operator<<(const T& x) { wstr << x; return *this; }
	std::wostringstream wstr;
};

// TODO: prefix with node name
class LogHandler : public prt::LogHandler {
public:
	LogHandler(const std::wstring& name, prt::LogLevel init) : mName(name), mLevel(init) { }

	virtual void handleLogEvent(const wchar_t* msg, prt::LogLevel level) {
		if (level >= mLevel) {
			// probably not the best idea - is there a houdini logging framework?
			std::wcout << L"[" << mName << L"] " << msg << std::endl;
		}
	}

	virtual const prt::LogLevel* getLevels(size_t* count) {
		*count = prt::LogHandler::ALL_COUNT;
		return prt::LogHandler::ALL;
	}

	virtual void getFormat(bool* dateTime, bool* level) {
		*dateTime = true;
		*level = true;
	}

	void setLevel(prt::LogLevel level) {
		mLevel = level;
	}

	void setLevel(const std::string& ls) {
		for (size_t i = 0; i < 6; i++) {
			if (ls == LEVELS[i]) {
				mLevel = static_cast<prt::LogLevel>(i);
				return;
			}
		}
		mLevel = prt::LOG_ERROR;
	}

	void setName(const std::wstring& n) {
		mName = n;
	}

private:
	std::wstring mName;
	prt::LogLevel mLevel;
};

} // namespace log
} // namespace p4h


// switch logger here
#define LT p4h::log::PRTLogger

typedef LT<prt::LOG_DEBUG>		_LOG_DBG;
typedef LT<prt::LOG_INFO>		_LOG_INF;
typedef LT<prt::LOG_WARNING>	_LOG_WRN;
typedef LT<prt::LOG_ERROR>		_LOG_ERR;

// convenience shortcuts in global namespace
#define LOG_DBG _LOG_DBG()
#define LOG_INF _LOG_INF()
#define LOG_WRN _LOG_WRN()
#define LOG_ERR _LOG_ERR()
