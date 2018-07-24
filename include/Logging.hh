/** \file
 *
 * \brief Logging utilities
 */

#ifndef LOGGING_HH_
#define LOGGING_HH_

#include <array>
#include <algorithm>
#include <cstdint>
#include <ostream>
#include <iterator>

#include "IoUtility.hh"

namespace Bridge {

/** \brief Log level
 *
 * \sa setupLogging(), log()
 */
enum class LogLevel {
    NONE,     ///< No logging
    FATAL,    ///< Unrecoverable error situations
    ERROR,    ///< Recoverable error situations
    WARNING,  ///< Unexpected concerning events
    INFO,     ///< Other events of importance
    DEBUG     ///< Verbose debugging logging
};

/// \cond DOXYGEN_IGNORE
/// These are helpers for implementing log()

namespace Impl {

bool shouldLog(LogLevel level);
std::ostream& logStream();

template<typename FormatIterator>
void log(FormatIterator first, FormatIterator last)
{
    if (first != last) {
        logStream() << std::addressof(*first);
    }
}

template<typename FormatIterator, typename First, typename... Rest>
void log(
    FormatIterator first, FormatIterator last, const First& arg,
    const Rest&... rest)
{
    const auto iter = std::find(first, last, '%');
    if (iter == last || std::next(iter) == last) {
        log(first, iter);
    } else {
        logStream().write(std::addressof(*first), iter - first);
        // This seems to be necessary to bring some operator<< overloads defined
        // in the Bridge namespace into consideration. I hope it doesn't have
        // any twisted side effects on the lookup...
        {
            using Bridge::operator<<;
            logStream() << arg;
        }
        log(std::next(iter, 2), last, rest...);
    }
}

}

/// \endcond

/** \brief Logging utility
 *
 * Log message if \p level is at least the minimum logging level set by
 * setupLogging().
 *
 * \note By design this utility is not thread safe. Only one thread should log
 * at a time.
 *
 * \note The \p format string is inspired by the standard C formatting
 * string. However, the implementation ignores the actual type specified by the
 * format specifiers, and instead the \p ts are streamed to the stream setup
 * using setupLogging() as is. Multi‐character specifiers are not understood and
 * exactly one character following the \% sign is always ignored. For clarity it
 * is encouraged, although not enforced, that the specifiers match the data type
 * of the corresponding value.
 *
 * \param level the logging level
 * \param format the formatting string
 * \param ts the values streamed to the placeholders in \p format
 */
template<typename String, typename... Ts>
void log(LogLevel level, const String& format, const Ts&... ts)
{
    if (Impl::shouldLog(level)) {
        Impl::log(std::begin(format), std::end(format), ts...);
        Impl::logStream() << '\n';
    }
}

/** \brief Default mapping between verbosity and logging level
 *
 * It is common convention that the verbosity of the output of an application
 * can be increased and decreased by specifying -v option one or more times when
 * invoking the application. This function provides the default mapping between
 * the number of times verbosity is specified and the logging level for the
 * verbosity.
 *
 * \param verbosity the verbosity level (typically the number of times -v flag
 * is given)
 *
 * \return LogLevel corresponding the verbosity (0 warning, 1 info, >=2 debug)
 */
LogLevel getLogLevel(int verbosity);

/** \brief Setup logging utility
 *
 * This function sets up the (global) minimum logging level and the stream to
 * which the log is output.
 *
 * If this method is not called, the default logging level is LogLevel::WARNING
 * and the default stream is std::cerr. If the logging level is set to
 * LogLevel::NONE, no logs are produced.
 *
 * The application is responsible for ensuring that no logging takes place after
 * the lifetime of \p stream has ended until another stream has been setup using
 * this method.
 *
 * \param level the minimum logging level that causes log to be output
 * \param stream the output stream to which the logs are output
 */
void setupLogging(LogLevel level, std::ostream& stream);

}

#endif // LOGGING_HH_
