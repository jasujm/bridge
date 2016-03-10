#include "messaging/FunctionMessageHandler.hh"
#include "messaging/SerializationFailureException.hh"

#include <boost/lexical_cast.hpp>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <iterator>
#include <string>
#include <tuple>
#include <vector>

using Bridge::Messaging::makeMessageHandler;
using Bridge::Messaging::MessageHandler;

using testing::Bool;
using testing::ElementsAreArray;
using testing::Return;

namespace {

using namespace std::string_literals;
const auto IDENTITY = "identity"s;
const auto REPLY = "reply"s;

std::string getReply(const std::string&)
{
    return REPLY;
}

class MockFunction {
public:
    MOCK_METHOD1(call0, bool(std::string));
    MOCK_METHOD2(call1, bool(std::string, std::string));
    MOCK_METHOD3(call2, bool(std::string, int, std::string));
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
    void testHelper(
        MessageHandler& handler,
        const std::vector<std::string> params,
        const bool expectedSuccess,
        const std::vector<std::string> expectedOutput = {})
    {
        EXPECT_EQ(
            expectedSuccess,
            handler.handle(
                IDENTITY, params.begin(), params.end(),
                std::back_inserter(output)));
        EXPECT_THAT(output, ElementsAreArray(expectedOutput));
    }

    std::vector<std::string> output;
    testing::StrictMock<MockFunction> function;
};

TEST_P(FunctionMessageHandlerTest, testNoParams)
{
    const auto success = GetParam();
    auto handler = makeMessageHandler(
        [this](const auto& identity)
        {
            return function.call0(identity);
        }, TestPolicy {});
    EXPECT_CALL(function, call0(IDENTITY)).WillOnce(Return(success));
    testHelper(*handler, {}, success);
}

TEST_P(FunctionMessageHandlerTest, testOneParam)
{
    const auto success = GetParam();
    auto handler = makeMessageHandler<std::string>(
        [this](const auto& identity, std::string param)
        {
            return function.call1(identity, param);
        }, TestPolicy {});
    EXPECT_CALL(function, call1(IDENTITY, "param")).WillOnce(Return(success));
    testHelper(*handler, {"param"}, success);
}

TEST_P(FunctionMessageHandlerTest, testTwoParams)
{
    const auto success = GetParam();
    auto handler = makeMessageHandler<int, std::string>(
        [this](const auto& identity, int param1, std::string param2)
        {
            return function.call2(identity, param1, param2);
        }, TestPolicy {});
    EXPECT_CALL(function, call2(IDENTITY, 1, "param")).WillOnce(Return(success));
    testHelper(*handler, {"1", "param"}, success);
}

TEST_F(FunctionMessageHandlerTest, testFailedSerialization)
{
    auto handler = makeMessageHandler<std::string>(
        [this](const auto& identity, std::string param)
        {
            return function.call1(identity, param);
        }, FailingPolicy {});
    const auto params = std::vector<std::string> { "param" };
    testHelper(*handler, {"param"}, false);
}

TEST_F(FunctionMessageHandlerTest, testInvalidNumberOfParameters)
{
    auto handler = makeMessageHandler(
        [this](const auto& identity)
        {
            return function.call0(identity);
        }, TestPolicy {});
    testHelper(*handler, {"invalid"}, false);
}

TEST_F(FunctionMessageHandlerTest, testGetReplySuccess)
{
    auto handler = makeMessageHandler(&getReply, TestPolicy {});
    testHelper(*handler, {}, true, {REPLY});
}

INSTANTIATE_TEST_CASE_P(
    SuccessFailure, FunctionMessageHandlerTest, Bool());
