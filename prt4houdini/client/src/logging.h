#ifndef __P4H_LOGGING__
#define __P4H_LOGGING__

#include "prt/API.h"

#include "boost/foreach.hpp"

#include <string>
#include <ostream>


namespace prt4hdn {
namespace log {

struct Logger {
};

// log to std streams
const std::wstring LEVELS[] = { L"trace", L"debug", L"info", L"warning", L"error", L"fatal" };
template<prt::LogLevel L> struct StreamLogger : public Logger {
	StreamLogger(std::wostream& out = std::wcout) : Logger(), mOut(out) { mOut << prefix(); }
	virtual ~StreamLogger() { mOut << std::endl; }
	StreamLogger<L>& operator<<(std::wostream&(*x)(std::wostream&)) { mOut << x; return *this; }
	StreamLogger<L>& operator<<(const std::string& x) { std::copy(x.begin(), x.end(), std::ostream_iterator<char, wchar_t>(mOut)); return *this; }
	template<typename T> StreamLogger<L>& operator<<(const T& x) { mOut << x; return *this; }
	static std::wstring prefix() { return L"[" + LEVELS[L] + L"] "; }
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
		BOOST_FOREACH(const T& x, v) {
			wstr << x << L" ";
		}
		wstr << L"]";
		return *this;
	}
	template<typename T> PRTLogger<L>& operator<<(const T& x) { wstr << x; return *this; }
	std::wostringstream wstr;
};


// TODO: implement a logger which logs into houdini's message system...

} // namespace log
} // namespace prt4hdn


// switch logger here
#define LT prt4hdn::log::PRTLogger

typedef LT<prt::LOG_DEBUG>		_LOG_DBG;
typedef LT<prt::LOG_INFO>		_LOG_INF;
typedef LT<prt::LOG_WARNING>	_LOG_WRN;
typedef LT<prt::LOG_ERROR>		_LOG_ERR;

// convenience shortcuts in global namespace
#define LOG_DBG _LOG_DBG()
#define LOG_INF _LOG_INF()
#define LOG_WRN _LOG_WRN()
#define LOG_ERR _LOG_ERR()


#endif // __P4H_LOGGING__
