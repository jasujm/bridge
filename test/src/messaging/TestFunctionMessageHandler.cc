#include "messaging/FunctionMessageHandler.hh"
#include "messaging/SerializationFailureException.hh"

#include <boost/lexical_cast.hpp>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <string>
#include <tuple>

using Bridge::Messaging::makeMessageHandler;
using Bridge::Messaging::MessageHandler;

using testing::Bool;
using testing::Return;

namespace {

using namespace std::string_literals;
const auto IDENTITY = "identity"s;

class MockFunction {
public:
    MOCK_METHOD0(call0, bool());
    MOCK_METHOD1(call1, bool(std::string));
    MOCK_METHOD2(call2, bool(int, std::string));
};

class TestPolicy {
public:
    template<typename T> std::string serialize(const T& t);
    template<typename T> T deserialize(const std::string& s);
};

template<typename T>
std::string TestPolicy::serialize(const T& t)
{
    return boost::lexical_cast<std::string>(t);
}

template<typename T>
T TestPolicy::deserialize(const std::string& s)
{
    return boost::lexical_cast<T>(s);
}

class FailingPolicy {
public:
    template<typename T> T deserialize(const std::string& s);
};

template<typename T>
T FailingPolicy::deserialize(const std::string&)
{
    throw Bridge::Messaging::SerializationFailureException {};
}

}

class FunctionMessageHandlerTest : public testing::TestWithParam<bool> {
protected:
    testing::StrictMock<MockFunction> function;
};

TEST_P(FunctionMessageHandlerTest, testNoParams)
{
    const auto success = GetParam();
    auto handler = makeMessageHandler(
        [this]() { return function.call0(); }, TestPolicy {});
    const auto params = std::vector<std::string> {};
    EXPECT_CALL(function, call0()).WillOnce(Return(success));
    EXPECT_EQ(success, handler->handle(IDENTITY, params.begin(), params.end()));
}

TEST_P(FunctionMessageHandlerTest, testOneParam)
{
    const auto success = GetParam();
    auto handler = makeMessageHandler<std::string>(
        [this](std::string param) { return function.call1(param); },
        TestPolicy {});
    const auto params = std::vector<std::string> { "param" };
    EXPECT_CALL(function, call1("param")).WillOnce(Return(success));
    EXPECT_EQ(success, handler->handle(IDENTITY, params.begin(), params.end()));
}

TEST_P(FunctionMessageHandlerTest, testTwoParams)
{
    const auto success = GetParam();
    auto handler = makeMessageHandler<int, std::string>(
        [this](int param1, std::string param2)
        {
            return function.call2(param1, param2);
        }, TestPolicy {});
    const auto params = std::vector<std::string> { "1", "param" };
    EXPECT_CALL(function, call2(1, "param")).WillOnce(Return(success));
    EXPECT_EQ(success, handler->handle(IDENTITY, params.begin(), params.end()));
}

TEST_F(FunctionMessageHandlerTest, testFailedSerialization)
{
    auto handler = makeMessageHandler<std::string>(
        [this](std::string param) { return function.call1(param); },
        FailingPolicy {});
    const auto params = std::vector<std::string> { "param" };
    EXPECT_FALSE(handler->handle(IDENTITY, params.begin(), params.end()));
}

TEST_F(FunctionMessageHandlerTest, testInvalidNumberOfParameters)
{
    auto handler = makeMessageHandler(
        [this]() { return function.call0(); }, TestPolicy {});
    const auto params = std::vector<std::string> { "invalid" };
    EXPECT_FALSE(handler->handle(IDENTITY, params.begin(), params.end()));
}

INSTANTIATE_TEST_CASE_P(
    SuccessFailure, FunctionMessageHandlerTest, Bool());
