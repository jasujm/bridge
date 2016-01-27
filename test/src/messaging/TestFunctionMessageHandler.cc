#include "messaging/FunctionMessageHandler.hh"

#include <boost/lexical_cast.hpp>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <string>
#include <tuple>

using Bridge::Messaging::makeMessageHandler;
using Bridge::Messaging::MessageHandler;

using testing::Return;

namespace {

class MockFunction {
public:
    MOCK_METHOD0(call0, std::tuple<int, std::string>());
    MOCK_METHOD1(call1, std::tuple<int>(std::string));
    MOCK_METHOD2(call2, std::tuple<>(int, std::string));
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

}

class FunctionMessageHandlerTest : public testing::Test {
protected:
    testing::StrictMock<MockFunction> function;
};

TEST_F(FunctionMessageHandlerTest, noParamsTwoReturnValues)
{
    auto handler = makeMessageHandler(
        [this]() { return function.call0(); }, TestPolicy {});
    const auto params = std::vector<std::string> {};
    const auto return_value = MessageHandler::ReturnValue { "1", "return" };
    EXPECT_CALL(function, call0())
        .WillOnce(Return(std::make_tuple(1, "return")));
    EXPECT_EQ(
        return_value,
        handler->handle(params.begin(), params.end()));
}

TEST_F(FunctionMessageHandlerTest, oneParamOneReturnValue)
{
    auto handler = makeMessageHandler<std::string>(
        [this](std::string param) { return function.call1(param); },
        TestPolicy {});
    const auto params = std::vector<std::string> { "param" };
    const auto return_value = MessageHandler::ReturnValue { "1" };
    EXPECT_CALL(function, call1("param")).WillOnce(Return(std::make_tuple(1)));
    EXPECT_EQ(
        return_value,
        handler->handle(params.begin(), params.end()));
}

TEST_F(FunctionMessageHandlerTest, twoParamsNoReturnValue)
{
    auto handler = makeMessageHandler<int, std::string>(
        [this](int param1, std::string param2)
        {
            return function.call2(param1, param2);
        }, TestPolicy {});
    const auto params = std::vector<std::string> { "1", "param" };
    const auto return_value = MessageHandler::ReturnValue {};
    EXPECT_CALL(function, call2(1, "param"))
        .WillOnce(Return(std::make_tuple()));
    EXPECT_EQ(
        return_value,
        handler->handle(params.begin(), params.end()));
}

TEST_F(FunctionMessageHandlerTest, invalidNumberOfParameters)
{
    auto handler = makeMessageHandler(
        [this]() { return function.call0(); }, TestPolicy {});
    const auto params = std::vector<std::string> { "invalid" };
    EXPECT_THROW(
        handler->handle(params.begin(), params.end()),
        Bridge::Messaging::MessageHandlingException);
}
