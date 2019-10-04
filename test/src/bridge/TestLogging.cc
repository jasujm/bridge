#include "Logging.hh"

#include <gtest/gtest.h>

#include <array>
#include <iostream>
#include <sstream>
#include <string>

namespace {
using namespace std::string_view_literals;
constexpr auto MESSAGE = "This is logging"sv;
}

class LoggingTest : public testing::Test {
protected:
    virtual void SetUp()
    {
        setupLogging(Bridge::LogLevel::WARNING, stream);
    }

    virtual void TearDown()
    {
        setupLogging(Bridge::LogLevel::NONE, std::cerr);
    }

    std::ostringstream stream;
};

TEST_F(LoggingTest, testLoggingWithTriggeringLevel)
{
    setupLogging(Bridge::LogLevel::INFO, stream);
    log(Bridge::LogLevel::INFO, "format %s format"sv, MESSAGE);
    EXPECT_NE(std::string::npos, stream.str().find(MESSAGE));
}

TEST_F(LoggingTest, testLoggingWithLevelNone)
{
    setupLogging(Bridge::LogLevel::NONE, stream);
    log(Bridge::LogLevel::FATAL, "%s"sv, MESSAGE);
    EXPECT_TRUE(stream.str().empty());
}

TEST_F(LoggingTest, testLoggingWithMissingFormatSpecifier)
{
    log(Bridge::LogLevel::WARNING, ""sv, MESSAGE);
    EXPECT_EQ(std::string::npos, stream.str().find(MESSAGE));
}

TEST_F(LoggingTest, testLoggingWithInvalidFormatSpecifier)
{
    log(Bridge::LogLevel::WARNING, "%"sv, MESSAGE);
    EXPECT_EQ(std::string::npos, stream.str().find(MESSAGE));
}

TEST_F(LoggingTest, testVerbosity)
{
    EXPECT_EQ(Bridge::LogLevel::WARNING, Bridge::getLogLevel(0));
    EXPECT_EQ(Bridge::LogLevel::INFO, Bridge::getLogLevel(1));
    EXPECT_EQ(Bridge::LogLevel::DEBUG, Bridge::getLogLevel(2));
}
