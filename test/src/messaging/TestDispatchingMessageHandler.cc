#include "messaging/DispatchingMessageHandler.hh"
#include "MockMessageHandler.hh"
#include "MockSerializationPolicy.hh"

#include <gtest/gtest.h>

#include <array>
#include <memory>
#include <string>

using Bridge::Messaging::MockMessageHandler;
using Bridge::Messaging::MockResponse;
using Bridge::Messaging::MockSerializationPolicy;
using Bridge::Messaging::Respond;
using testing::_;
using testing::ElementsAre;
using testing::Ref;
using testing::Return;

using Bridge::asBytes;
using Bridge::Blob;
using Bridge::stringToBlob;
using Bridge::Messaging::REPLY_SUCCESS;
using Bridge::Messaging::REPLY_FAILURE;
using namespace std::string_literals;
using namespace Bridge::BlobLiterals;

namespace {

const auto IDENTITY = Bridge::Messaging::Identity {""s, "identity"_B};
const auto KEY = "key"_B;
const auto HANDLER1 = "handler1"s;
const auto HANDLER2 = "handler2"s;

}

class DispatchingMessageHandlerTest : public testing::Test {
protected:
    using HandlerType = Bridge::Messaging::DispatchingMessageHandler<
        std::string, MockSerializationPolicy>;

    std::shared_ptr<testing::StrictMock<MockMessageHandler>> delegate {
        std::make_shared<testing::StrictMock<MockMessageHandler>>()};
    testing::StrictMock<MockResponse> response;
    HandlerType handler {
        KEY, MockSerializationPolicy {}, {{ HANDLER1, delegate }}};
};

TEST_F(DispatchingMessageHandlerTest, testNoDelegateParameter)
{
    const auto params = std::array<Blob, 0> {};
    EXPECT_CALL(response, handleSetStatus(REPLY_FAILURE));
    handler.handle(
        {}, IDENTITY, params.begin(), params.end(), response);
}

TEST_F(DispatchingMessageHandlerTest, testInvalidMatchingParameter)
{
    const auto params = std::array { KEY };
    EXPECT_CALL(response, handleSetStatus(REPLY_FAILURE));
    handler.handle(
        {}, IDENTITY, params.begin(), params.end(), response);
}

TEST_F(DispatchingMessageHandlerTest, testNonexistingDelegate)
{
    const auto params = std::array { KEY, stringToBlob(HANDLER2) };
    EXPECT_CALL(response, handleSetStatus(REPLY_FAILURE));
    handler.handle(
        {}, IDENTITY, params.begin(), params.end(), response);
}

TEST_F(DispatchingMessageHandlerTest, testDelegate)
{
    const auto params = std::array { KEY, stringToBlob(HANDLER1) };
    EXPECT_CALL(response, handleSetStatus(REPLY_SUCCESS));
    EXPECT_CALL(
        *delegate,
        doHandle(
            _, IDENTITY,
            ElementsAre(asBytes(KEY), asBytes(HANDLER1)), Ref(response)))
        .WillOnce(Respond(REPLY_SUCCESS));
    handler.handle(
        {}, IDENTITY, params.begin(), params.end(), response);
}

TEST_F(DispatchingMessageHandlerTest, testAddDelegate)
{
    auto other_delegate {
        std::make_shared<testing::StrictMock<MockMessageHandler>>()};
    EXPECT_TRUE(handler.trySetDelegate(HANDLER2, other_delegate));
    const auto params = std::array { KEY, stringToBlob(HANDLER2) };
    EXPECT_CALL(response, handleSetStatus(REPLY_SUCCESS));
    EXPECT_CALL(
        *other_delegate,
        doHandle(
            _, IDENTITY,
            ElementsAre(asBytes(KEY), asBytes(HANDLER2)), Ref(response))).
        WillOnce(Respond(REPLY_SUCCESS));
    handler.handle(
        {}, IDENTITY, params.begin(), params.end(), response);
}

TEST_F(DispatchingMessageHandlerTest, testAddDelegateWithExistingKey)
{
    auto other_delegate {
        std::make_shared<testing::StrictMock<MockMessageHandler>>()};
    EXPECT_FALSE(handler.trySetDelegate(HANDLER1, other_delegate));
}
