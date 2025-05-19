#pragma once
#include <atomic>
#include <experimental/source_location>
using namespace std::experimental::fundamentals_v2;

#include "eNET-types.h"
#include "utilities.h"

#define LOG_DISABLE_TRACE
//#define LOG_DISABLE_INFO
#define LOG_DISABLE_DEBUG
//#define LOG_DISABLE_ERROR
#define LOG_DISABLE_FUNCTION_TRACE

extern int FUNCTION_DEPTH;

std::string elapsedms();
enum class LogLevel { Error = 0, Warning, Info, Debug, Trace };

void SetLogLevel(LogLevel level);
LogLevel GetLogLevel();

void LogImpl(LogLevel level, std::string message, const source_location &loc = source_location::current());
void LogImpl(LogLevel level, std::string intro, TBytes bytes, bool crlf = true, const source_location &loc = source_location::current());

#define LOG_IF(level, ...) \
    do { if ((level) <= GetLogLevel()) LogImpl(level, __VA_ARGS__); } while (0)

#define LOG_TRACE(...)   LOG_IF(LogLevel::Trace, __VA_ARGS__)
#define LOG_DEBUG(...)   LOG_IF(LogLevel::Debug, __VA_ARGS__)
#define LOG_INFO(...)    LOG_IF(LogLevel::Info,  __VA_ARGS__)
#define LOG_WARNING(...) LOG_IF(LogLevel::Warning, __VA_ARGS__)
#define LOG_ERROR(...)   LOG_IF(LogLevel::Error, __VA_ARGS__)

#define Trace(...)   LOG_TRACE(__VA_ARGS__)
#define Debug(...)   LOG_DEBUG(__VA_ARGS__)
#define Log(...)     LOG_INFO(__VA_ARGS__)
#define Warn(...)    LOG_WARNING(__VA_ARGS__)
#define Error(...)   LOG_ERROR(__VA_ARGS__)



#ifdef LOG_DISABLE_FUNCTION_TRACE
#define LOG_IT
#else
#define LOG_IT FunctionLogger logger_instance(__func__)


#pragma GCC push_options

class FunctionLogger {
public:
    __attribute__((no_instrument_function)) FunctionLogger(const char* func_name, const source_location &loc = source_location::current()) : function_name(func_name),location(loc) {
        ++FUNCTION_DEPTH;
        std::string prefix(FUNCTION_DEPTH, '-');
        printf("\033[36m[ LogIt ] %12s \033[32m%23s %-23s\033[0m %s %s (%5d)\n", elapsedms().c_str(), location.file_name(), "ENTER", prefix.c_str(), location.function_name(), location.line());
    }

    __attribute__((no_instrument_function)) ~FunctionLogger() {
        std::string prefix(FUNCTION_DEPTH, '-');
        printf("\033[36m[ LogIt ] %12s \033[32m%23s %-23s\033[0m %s %s (%5d)\n", elapsedms().c_str(), location.file_name(),"EXIT", prefix.c_str(), location.function_name(), location.line());
        --FUNCTION_DEPTH;
    }

private:
    const char* function_name;
    const source_location location;
};
#pragma GCC pop_options
#endif

