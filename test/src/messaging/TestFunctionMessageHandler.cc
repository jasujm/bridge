#include "messaging/FunctionMessageHandler.hh"
#include "messaging/SerializationFailureException.hh"
#include "MockMessageHandler.hh"
#include "MockSerializationPolicy.hh"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <iterator>
#include <string>
#include <tuple>
#include <vector>

using Bridge::Blob;
using namespace Bridge::Messaging;

using testing::Bool;
using testing::ElementsAreArray;
using testing::Return;

namespace Bridge {
namespace Messaging {

// TODO: Are output operators for reply types generally useful?

std::ostream& operator<<(std::ostream& os, ReplyFailure)
{
    return os << "reply failure";
}

template<typename... Ts>
std::ostream& operator<<(std::ostream& os, ReplySuccess<Ts...>)
{
    return os << "reply success";
}

}
}

namespace {

using namespace std::string_literals;
using namespace Bridge::BlobLiterals;
const auto IDENTITY = Identity { ""s, "identity"_B };
const auto KEY1 = "key1"s;
const auto KEY2 = "key2"s;
const auto REPLY_KEY1 = "replykey1"s;
const auto REPLY_KEY2 = "replykey2"s;
const auto REPLY1 = "reply"s;
const auto REPLY2 = 3;

Reply<> makeReply(const bool successful)
{
    if (successful) {
        return success();
    }
    return failure();
}

Reply<std::string> reply1(const Identity&)
{
    return success(REPLY1);
}

Reply<std::string, int> reply2(const Identity&)
{
    return success(REPLY1, REPLY2);
}

Reply<std::optional<std::string>> replyOptional(const Identity&)
{
    return success(REPLY1);
}

Reply<std::optional<std::string>> replyNone(const Identity&)
{
    return success(std::nullopt);
}

class MockFunction {
public:
    MOCK_METHOD1(call0, Reply<>(Identity));
    MOCK_METHOD2(call1, Reply<>(Identity, std::string));
    MOCK_METHOD3(call2, Reply<>(Identity, int, std::string));
    MOCK_METHOD2(call_opt, Reply<>(Identity, std::optional<int>));
};

class FailingPolicy {
public:
    template<typename T> T deserialize(Bridge::ByteSpan s);
};

template<typename T>
T FailingPolicy::deserialize(Bridge::ByteSpan)
{
    throw Bridge::Messaging::SerializationFailureException {};
}

}

class FunctionMessageHandlerTest : public testing::TestWithParam<bool> {
protected:
    void testHelper(
        MessageHandler& handler,
        const std::vector<std::string> params,
        const StatusCode expectedStatus,
        const std::vector<std::string> expectedOutput = {})
    {
        Bridge::Messaging::SynchronousExecutionPolicy execution;
        EXPECT_CALL(response, handleSetStatus(expectedStatus));
        {
            testing::InSequence guard;
            for (const auto& output : expectedOutput) {
                EXPECT_CALL(response, handleAddFrame(Bridge::asBytes(output)));
            }
        }
        handler.handle(
            execution, IDENTITY, params.begin(), params.end(), response);
    }

    testing::StrictMock<MockResponse> response;
    testing::StrictMock<MockFunction> function;
};

TEST_P(FunctionMessageHandlerTest, testNoParams)
{
    const auto success = GetParam();
    auto handler = makeMessageHandler(
        [this](const auto& identity)
        {
            return function.call0(identity);
        }, MockSerializationPolicy {});
    EXPECT_CALL(function, call0(IDENTITY)).WillOnce(Return(makeReply(success)));
    testHelper(*handler, {}, success ? REPLY_SUCCESS : REPLY_FAILURE);
}

TEST_P(FunctionMessageHandlerTest, testOneParam)
{
    const auto success = GetParam();
    auto handler = makeMessageHandler<std::string>(
        [this](const auto& identity, std::string param)
        {
            return function.call1(identity, param);
        }, MockSerializationPolicy {}, std::make_tuple(KEY1));
    EXPECT_CALL(function, call1(IDENTITY, "param")).WillOnce(Return(makeReply(success)));
    testHelper(*handler, {KEY1, "param"}, success ? REPLY_SUCCESS : REPLY_FAILURE);
}

TEST_P(FunctionMessageHandlerTest, testTwoParams)
{
    const auto success = GetParam();
    auto handler = makeMessageHandler<int, std::string>(
        [this](const auto& identity, int param1, std::string param2)
        {
            return function.call2(identity, param1, param2);
        }, MockSerializationPolicy {}, std::make_tuple(KEY1, KEY2));
    EXPECT_CALL(function, call2(IDENTITY, 1, "param")).WillOnce(Return(makeReply(success)));
    testHelper(*handler, {KEY1, "1", KEY2, "param"}, success ? REPLY_SUCCESS : REPLY_FAILURE);
}

TEST_F(FunctionMessageHandlerTest, testFailedSerialization)
{
    auto handler = makeMessageHandler<std::string>(
        [this](const auto& identity, std::string param)
        {
            return function.call1(identity, param);
        }, FailingPolicy {}, std::make_tuple(KEY1));
    testHelper(*handler, {KEY1, "param"}, REPLY_FAILURE);
}

TEST_F(FunctionMessageHandlerTest, testMissingParameters)
{
    auto handler = makeMessageHandler<std::string>(
        [this](const auto& identity, std::string param)
        {
            return function.call1(identity, param);
        }, MockSerializationPolicy {}, std::make_tuple(KEY1));
    testHelper(*handler, {}, REPLY_FAILURE);
}

TEST_F(FunctionMessageHandlerTest, testExtraParameters)
{
    auto handler = makeMessageHandler<std::string>(
        [this](const auto& identity, std::string param)
        {
            return function.call1(identity, param);
        }, MockSerializationPolicy {}, std::make_tuple(KEY1));
    EXPECT_CALL(function, call1(IDENTITY, "param"))
        .WillOnce(Return(makeReply(true)));
    testHelper(*handler, {KEY1, "param", KEY2, "1"}, REPLY_SUCCESS);
}

TEST_F(FunctionMessageHandlerTest, testNoKey)
{
    auto handler = makeMessageHandler<std::string>(
        [this](const auto& identity, std::string param)
        {
            return function.call1(identity, param);
        }, MockSerializationPolicy {}, std::make_tuple(KEY1));
    testHelper(*handler, {"invalid"}, REPLY_FAILURE);
}

TEST_F(FunctionMessageHandlerTest, testInvalidKey)
{
    auto handler = makeMessageHandler<std::string>(
        [this](const auto& identity, std::string param)
        {
            return function.call1(identity, param);
        }, MockSerializationPolicy {}, std::make_tuple(KEY1));
    testHelper(*handler, {KEY2, "invalid"}, REPLY_FAILURE);
}

TEST_F(FunctionMessageHandlerTest, testOptionalParamPresent)
{
    auto handler = makeMessageHandler<std::optional<int>>(
        [this](const auto& identity, std::optional<int> param)
        {
            return function.call_opt(identity, param);
        }, MockSerializationPolicy {}, std::make_tuple(KEY1));
    EXPECT_CALL(function, call_opt(IDENTITY, std::make_optional(123)))
        .WillOnce(Return(makeReply(true)));
    testHelper(*handler, {KEY1, "123"}, REPLY_SUCCESS);
}

TEST_F(FunctionMessageHandlerTest, testOptionalParamNotPresent)
{
    auto handler = makeMessageHandler<std::optional<int>>(
        [this](const auto& identity, std::optional<int> param)
        {
            return function.call_opt(identity, param);
        }, MockSerializationPolicy {}, std::make_tuple(KEY1));
    EXPECT_CALL(function, call_opt(IDENTITY, std::optional<int> {}))
        .WillOnce(Return(makeReply(true)));
    testHelper(*handler, {}, REPLY_SUCCESS);
}

TEST_F(FunctionMessageHandlerTest, testGetReply1)
{
    auto handler = makeMessageHandler(
        &reply1, MockSerializationPolicy {},
        std::make_tuple(), std::make_tuple(REPLY_KEY1));
    testHelper(*handler, {}, REPLY_SUCCESS, {REPLY_KEY1, REPLY1});
}

TEST_F(FunctionMessageHandlerTest, testGetReply2)
{
    auto handler = makeMessageHandler(
        &reply2, MockSerializationPolicy {},
        std::make_tuple(), std::make_tuple(REPLY_KEY1, REPLY_KEY2));
    testHelper(
        *handler, {}, REPLY_SUCCESS,
        {REPLY_KEY1, REPLY1, REPLY_KEY2, boost::lexical_cast<std::string>(REPLY2)});
}

TEST_F(FunctionMessageHandlerTest, testGetReplyOptional)
{
    auto handler = makeMessageHandler(
        &replyOptional, MockSerializationPolicy {},
        std::make_tuple(), std::make_tuple(REPLY_KEY1));
    testHelper(*handler, {}, REPLY_SUCCESS, {REPLY_KEY1, REPLY1});
}

TEST_F(FunctionMessageHandlerTest, testGetReplyNone)
{
    auto handler = makeMessageHandler(
        &replyNone, MockSerializationPolicy {},
        std::make_tuple(), std::make_tuple(REPLY_KEY1));
    testHelper(*handler, {}, REPLY_SUCCESS, {});
}

INSTANTIATE_TEST_CASE_P(
    SuccessFailure, FunctionMessageHandlerTest, Bool());
