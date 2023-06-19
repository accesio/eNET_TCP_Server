#include <string>
// #include <memory>
// #include <sstream>
// #include <iostream>
// #include <iomanip>
#include <stdio.h>
// #include <linux/types.h>

#include "logging.h"

timespec firsttime;
int FUNCTION_DEPTH = 0;
std::string elapsedms() __attribute__((no_instrument_function));
std::string elapsedms()
{
	struct timespec up;
	clock_gettime(CLOCK_BOOTTIME, &up);
	if(firsttime.tv_nsec == 0)
		firsttime = up;
	double now = double(up.tv_sec-firsttime.tv_sec) + double(up.tv_nsec-firsttime.tv_nsec) / 1.0e9;
	std::string str = std::to_string(now)+"s";
	return str;
}

// "\033[33m[Warning]\033[0m ";


#define RED "\033[31m"
#define YELLOW "\033[32m"
#define CYAN "\033[36m"
#define logtext "%s[ %5s ] %12s %23s %23s\033[0m %s (%5d) %s"
#define logCR "%s"

#ifndef LOG_DISABLE_INFO
int Log(const std::string message, const source_location &loc)
{
	std::string prefix(FUNCTION_DEPTH, '-');
	printf(logtext "\n", CYAN, "INFO ", elapsedms().c_str(), loc.file_name(), loc.function_name(), prefix.c_str(), loc.line(), message.c_str());
	return 0;
}

int Log(const std::string intro, const TBytes bytes, bool crlf, const source_location &loc)
{
	std::string prefix(FUNCTION_DEPTH, '-');
	std::stringstream msg;
	msg << intro;
	for (auto byt : bytes)
		msg << std::hex << std::setfill('0') << std::setw(2) << std::uppercase << static_cast<int>(byt) << " ";
	printf(logtext logCR, CYAN, "INFO ", elapsedms().c_str(), loc.file_name(), loc.function_name(), prefix.c_str(), loc.line(), msg.str().c_str(), crlf?"\n":"");
	return 0;
}
#endif

#ifndef LOG_DISABLE_TRACE
int Trace(const std::string message, const source_location &loc)
{
	std::string prefix(FUNCTION_DEPTH, '-');
	printf(logtext "\n", CYAN, "TRACE", elapsedms().c_str(), loc.file_name(), loc.function_name(), prefix.c_str(), loc.line(), message.c_str());
	return 0;
}

int Trace(const std::string intro, const TBytes bytes, bool crlf, const source_location &loc)
{
	std::string prefix(FUNCTION_DEPTH, '-');
	std::stringstream msg;
	msg << intro;
	for (auto byt : bytes)
		msg << std::hex << std::setfill('0') << std::setw(2) << std::uppercase << static_cast<int>(byt) << " ";
	printf(logtext logCR, CYAN, "TRACE", elapsedms().c_str(), loc.file_name(), loc.function_name(), prefix.c_str(), loc.line(), msg.str().c_str(), crlf?"\n":"");
	return 0;
}
#endif

#ifndef LOG_DISABLE_DEBUG
int Debug(const std::string message, const source_location &loc)
{
	std::string prefix(FUNCTION_DEPTH, '-');
	printf(logtext "\n", YELLOW, "DEBUG", elapsedms().c_str(), loc.file_name(), loc.function_name(), prefix.c_str(), loc.line(), message.c_str());
	return 0;
}

int Debug(const std::string intro, const TBytes bytes, bool crlf, const source_location &loc)
{	std::string prefix(FUNCTION_DEPTH, '-');

	std::stringstream msg;
	msg << intro;
	for (auto byt : bytes)
		msg << std::hex << std::setfill('0') << std::setw(2) << std::uppercase << static_cast<int>(byt) << " ";
	printf(logtext logCR, YELLOW, "DEBUG", elapsedms().c_str(), loc.file_name(), loc.function_name(), prefix.c_str(), loc.line(), msg.str().c_str(), crlf?"\n":"");
	return 0;
}
#endif

int Error(const std::string message, const source_location &loc)
{
	std::string prefix(FUNCTION_DEPTH, '-');
	printf(logtext, RED, "ERROR", elapsedms().c_str(), loc.file_name(), loc.function_name(), prefix.c_str(), loc.line(), message.c_str());
	return 0;
}

int Error(const std::string intro, const TBytes bytes, bool crlf, const source_location &loc)
{
	std::string prefix(FUNCTION_DEPTH, '-');
	std::stringstream msg;
	msg << intro;
	for (auto byt : bytes)
		msg << std::hex << std::setfill('0') << std::setw(2) << std::uppercase << static_cast<int>(byt) << " ";
	printf(logtext logCR, RED, "ERROR", elapsedms().c_str(), loc.file_name(), loc.function_name(), prefix.c_str(), loc.line(), msg.str().c_str(), crlf?"\n":"");

	return 0;
}
