#include <string>
#include <stdio.h>
#include <atomic>
#include <experimental/source_location>
using namespace std::experimental::fundamentals_v2;
#include "logging.h"

timespec firsttime;
static std::atomic<LogLevel> currentLogLevel{LogLevel::Info};
int FUNCTION_DEPTH = 0;
std::string elapsedms() __attribute__((no_instrument_function));
std::string elapsedms()
{
	struct timespec up;
	clock_gettime(CLOCK_BOOTTIME, &up);
	if(firsttime.tv_nsec == 0)
		firsttime = up;
	double now = double(up.tv_sec-firsttime.tv_sec) + double(up.tv_nsec-firsttime.tv_nsec) / 1.0e9;
	return std::to_string(now)+"s";

}

// "\033[33m[Warning]\033[0m ";


#define RED "\033[31m"
#define YELLOW "\033[32m"
#define CYAN "\033[36m"
#define logtext "%s[ %5s ] %12s %23s %23s\033[0m %s (%5d) %s"
#define logCR "%s"

void SetLogLevel(LogLevel level)
{
	currentLogLevel.store(level);
}

LogLevel GetLogLevel()
{
	return currentLogLevel.load();
}


static const char* labelForLevel(LogLevel level)
{
    switch(level) {
		case LogLevel::Error:   return "[ ERROR ]";
		case LogLevel::Warning: return "[WARNING]";
		case LogLevel::Info:    return "[ INFO  ]";
		case LogLevel::Debug:   return "[ DEBUG ]";
		case LogLevel::Trace:   return "[ TRACE ]";
    }
    return "[??????]";
}

static const char* colorForLevel(LogLevel level)
{
    switch(level) {
		case LogLevel::Error:   return "\033[31m"; // red
		case LogLevel::Warning: return "\033[33m"; // yellow
		case LogLevel::Info:    return "\033[32m"; // green
		case LogLevel::Debug:   return "";         // default
		case LogLevel::Trace:   return "\033[36m"; // cyan
    }
    return "";
}
void LogImpl(LogLevel level, std::string intro, TBytes bytes, bool crlf, const source_location &loc)
{
    std::stringstream msg;
    msg << intro;
    for (auto byt : bytes)
        msg << std::hex << std::setfill('0') << std::setw(2)
            << std::uppercase << static_cast<int>(byt) << ' ';
    const char* color = colorForLevel(level);
    const char* label = labelForLevel(level);
    printf("%s%s\033[0m %14s \033[32m%13s %s\033[0m (%5d) %s%s",
           color, label, elapsedms().c_str(),
           loc.file_name(), loc.function_name(), loc.line(),
           msg.str().c_str(), crlf ? "\n" : "");
}

void LogImpl(LogLevel level, std::string message, const source_location &loc)
{
    // forward to the full overload with an empty payload
    LogImpl(level,
            std::move(message),
            TBytes{} /*empty bytes*/,
            true     /*default crlf*/,
            loc);
}