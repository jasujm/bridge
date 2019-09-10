#include "Logging.hh"

#include <ctime>
#include <functional>
#include <iomanip>
#include <iostream>
#include <map>

namespace Bridge {

namespace {
auto globalLoggingStream = std::ref(std::cerr);
auto globalLoggingLevel = LogLevel::WARNING;
}

namespace Impl {

bool shouldLog(LogLevel level)
{
    using namespace std::string_view_literals;
    static const std::map<LogLevel, std::string_view> DEBUG_LEVEL_MAP {
        { LogLevel::FATAL,   "FATAL   "sv },
        { LogLevel::ERROR,   "ERROR   "sv },
        { LogLevel::WARNING, "WARNING "sv },
        { LogLevel::INFO,    "INFO    "sv },
        { LogLevel::DEBUG,   "DEBUG   "sv },
    };

    if (level <= globalLoggingLevel) {
        const auto time = std::time(nullptr);
        logStream() << std::put_time(std::localtime(&time), "%c ") <<
            DEBUG_LEVEL_MAP.at(level);
        return true;
    }
    return false;
}

std::ostream& logStream()
{
    return globalLoggingStream;
}

}

LogLevel getLogLevel(int verbosity)
{
    if (verbosity >= 2) {
        return LogLevel::DEBUG;
    } else if (verbosity == 1) {
        return LogLevel::INFO;
    }
    return LogLevel::WARNING;
}

void setupLogging(LogLevel level, std::ostream& stream)
{
    globalLoggingLevel = level;
    globalLoggingStream = stream;
}

}
