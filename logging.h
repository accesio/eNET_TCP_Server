#pragma once
#include <experimental/source_location>
using namespace std::experimental;

#include "utilities.h"

//#define LOG_DISABLE_TRACE
//#define LOG_DISABLE_INFO
// #define LOG_DISABLE_DEBUG
// #define LOG_DISABLE_ERROR
//#define LOG_DISABLE_FUNCTION_TRACE

extern int FUNCTION_DEPTH;

std::string elapsedms();

// logging levels can be disabled at compile time
#ifdef LOG_DISABLE_TRACE
#define Trace(...) {}
#else
int Trace(std::string message, const source_location &loc = source_location::current());
int Trace(std::string intro, TBytes bytes, bool crlf = true, const source_location &loc = source_location::current());
#endif

#ifdef LOG_DISABLE_WARNING
#define Warn(...) {}
#endif

#ifdef LOG_DISABLE_INFO
#define Log(...) {}
#else
int Log(  std::string message, const source_location &loc = source_location::current());
int Log(  std::string intro, TBytes bytes, bool crlf = true, const source_location &loc = source_location::current());
#endif

#ifdef LOG_DISABLE_DEBUG
#define Debug(...) {}
#else
int Debug(std::string message, const source_location &loc = source_location::current());
int Debug(std::string intro, TBytes bytes, bool crlf = true, const source_location &loc = source_location::current());
#endif

int Error(std::string message, const source_location &loc = source_location::current());
int Error(std::string intro, TBytes bytes, bool crlf = true, const source_location &loc = source_location::current());

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

