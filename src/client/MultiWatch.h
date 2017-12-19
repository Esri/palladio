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
