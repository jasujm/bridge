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
    static const std::map<LogLevel, const char*> DEBUG_LEVEL_MAP {
        { LogLevel::FATAL,   "FATAL   " },
        { LogLevel::ERROR,   "ERROR   " },
        { LogLevel::WARNING, "WARNING " },
        { LogLevel::INFO,    "INFO    " },
        { LogLevel::DEBUG,   "DEBUG   " },
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

void setupLogging(LogLevel level, std::ostream& stream)
{
    globalLoggingLevel = level;
    globalLoggingStream = stream;
}

}
