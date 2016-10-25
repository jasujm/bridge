#include "Logging.hh"

#include <gtest/gtest.h>

#include <array>
#include <iostream>
#include <sstream>
#include <string>

namespace {
using namespace std::string_literals;
const auto MESSAGE = "This is logging"s;
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
    log(Bridge::LogLevel::INFO, "format %s format"s, MESSAGE);
    std::cout << stream.str() << stream.str().size() << std::endl;
    EXPECT_NE(std::string::npos, stream.str().find(MESSAGE));
}

TEST_F(LoggingTest, testLoggingWithLevelNone)
{
    setupLogging(Bridge::LogLevel::NONE, stream);
    log(Bridge::LogLevel::FATAL, "%s"s, MESSAGE);
    EXPECT_TRUE(stream.str().empty());
}

TEST_F(LoggingTest, testLoggingWithMissingFormatSpecifier)
{
    log(Bridge::LogLevel::WARNING, ""s, MESSAGE);
    EXPECT_EQ(std::string::npos, stream.str().find(MESSAGE));
}

TEST_F(LoggingTest, testLoggingWithInvalidFormatSpecifier)
{
    log(Bridge::LogLevel::WARNING, "%"s, MESSAGE);
    EXPECT_EQ(std::string::npos, stream.str().find(MESSAGE));
}

TEST_F(LoggingTest, testHexFormatting)
{
    std::array<unsigned char, 8> data {
        { 0x01, 0x23, 0x45, 0x67, 0x89, 0xab, 0xcd, 0xef }};
    log(Bridge::LogLevel::WARNING, "%s"s, Bridge::asHex(data));
    EXPECT_NE(std::string::npos, stream.str().find("0123456789abcdef"));
}
