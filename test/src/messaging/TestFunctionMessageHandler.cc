#include "messaging/FunctionMessageHandler.hh"
#include "messaging/SerializationFailureException.hh"

#include <boost/lexical_cast.hpp>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <iterator>
#include <string>
#include <tuple>
#include <vector>

using Bridge::Messaging::MessageHandler;
using Bridge::Messaging::Reply;
using Bridge::Messaging::failure;
using Bridge::Messaging::makeMessageHandler;
using Bridge::Messaging::success;

using testing::Bool;
using testing::ElementsAreArray;
using testing::Return;

namespace Bridge {
namespace Messaging {

// TODO: Are output operators for reply types generally useful?

std::ostream& operator<<(std::ostream& os, Bridge::Messaging::ReplyFailure)
{
    return os << "reply failure";
}

template<typename... Ts>
std::ostream& operator<<(
    std::ostream& os, Bridge::Messaging::ReplySuccess<Ts...>)
{
    return os << "reply success";
}

}
}

namespace {

using namespace std::string_literals;
const auto IDENTITY = "identity"s;
const auto REPLY1 = "reply"s;
const auto REPLY2 = 3;

Reply<> makeReply(const bool successful)
{
    if (successful) {
        return success();
    }
    return failure();
}

Reply<std::string> reply1(const std::string&)
{
    return success(REPLY1);
}

Reply<std::string, int> reply2(const std::string&)
{
    return success(REPLY1, REPLY2);
}

class MockFunction {
public:
    MOCK_METHOD1(call0, Reply<>(std::string));
    MOCK_METHOD2(call1, Reply<>(std::string, std::string));
    MOCK_METHOD3(call2, Reply<>(std::string, int, std::string));
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
    EXPECT_CALL(function, call0(IDENTITY)).WillOnce(Return(makeReply(success)));
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
    EXPECT_CALL(function, call1(IDENTITY, "param")).WillOnce(Return(makeReply(success)));
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
    EXPECT_CALL(function, call2(IDENTITY, 1, "param")).WillOnce(Return(makeReply(success)));
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

TEST_F(FunctionMessageHandlerTest, testGetReply1)
{
    auto handler = makeMessageHandler(&reply1, TestPolicy {});
    testHelper(*handler, {}, true, {REPLY1});
}

TEST_F(FunctionMessageHandlerTest, testGetReply2)
{
    auto handler = makeMessageHandler(&reply2, TestPolicy {});
    testHelper(
        *handler, {}, true, {REPLY1, boost::lexical_cast<std::string>(REPLY2)});
}

INSTANTIATE_TEST_CASE_P(
    SuccessFailure, FunctionMessageHandlerTest, Bool());
