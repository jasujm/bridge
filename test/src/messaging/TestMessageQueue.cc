#include "messaging/FunctionMessageHandler.hh"
#include "messaging/MessageHandler.hh"
#include "messaging/MessageQueue.hh"
#include "messaging/MessageUtility.hh"
#include "messaging/Replies.hh"
#include "messaging/Sockets.hh"
#include "MockMessageHandler.hh"
#include "Blob.hh"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <map>
#include <string>
#include <utility>

using testing::_;
using testing::ElementsAre;
using testing::Invoke;
using testing::Return;

using namespace Bridge::Messaging;

namespace {
using namespace std::string_literals;
using namespace std::string_view_literals;
using namespace Bridge::BlobLiterals;
const auto IDENTITY = Identity { ""s, "identity"_B };
const auto PARAM1 = "param1"_BS;
const auto PARAM2 = "param2"_BS;
constexpr auto ENDPOINT = "inproc://testing"sv;
const auto TAG = "tag"_BS;
const auto COMMAND = "cmd"_BS;
const auto OTHER_COMMAND = "cmd2"_BS;
}

class MessageQueueTest : public testing::Test {
protected:
    virtual void SetUp()
    {
        messageQueue.trySetHandler(COMMAND, handler);
        bindSocket(backSocket, ENDPOINT);
        frontSocket.setsockopt(
            ZMQ_ROUTING_ID, IDENTITY.routingId.data(), IDENTITY.routingId.size());
        connectSocket(frontSocket, ENDPOINT);
    }

    void assertReply(bool success, bool more = false)
    {
        auto message = Message {};
        recvMessage(frontSocket, message);
        EXPECT_EQ(TAG, messageView(message));
        ASSERT_TRUE(message.more());
        recvMessage(frontSocket, message);
        EXPECT_EQ(success, isSuccessful(messageView(message)));
        EXPECT_EQ(more, message.more());
    }

    MessageContext context;
    Socket frontSocket {context, SocketType::req};
    Socket backSocket {context, SocketType::router};
    std::shared_ptr<MockMessageHandler> handler {
        std::make_shared<MockMessageHandler>()};
    MessageQueue messageQueue;
};

TEST_F(MessageQueueTest, testValidCommandInvokesCorrectHandlerSuccessful)
{
    EXPECT_CALL(*handler, doHandle(_, IDENTITY, ElementsAre(PARAM1, PARAM2), _))
        .WillOnce(Respond(REPLY_SUCCESS));
    sendMessage(frontSocket, messageBuffer(TAG), true);
    sendMessage(frontSocket, messageBuffer(COMMAND), true);
    sendMessage(frontSocket, messageBuffer(PARAM1), true);
    sendMessage(frontSocket, messageBuffer(PARAM2));

    messageQueue(backSocket);
    assertReply(true);
}

TEST_F(MessageQueueTest, testValidCommandInvokesCorrectHandlerFailure)
{
    EXPECT_CALL(*handler, doHandle(_, IDENTITY, ElementsAre(PARAM1, PARAM2), _))
        .WillOnce(Respond(REPLY_FAILURE));
    sendMessage(frontSocket, messageBuffer(TAG), true);
    sendMessage(frontSocket, messageBuffer(COMMAND), true);
    sendMessage(frontSocket, messageBuffer(PARAM1), true);
    sendMessage(frontSocket, messageBuffer(PARAM2));

    messageQueue(backSocket);
    assertReply(false);
}

TEST_F(MessageQueueTest, testInvalidCommandReturnsError)
{
    EXPECT_CALL(*handler, doHandle(_, _, _, _)).Times(0);
    sendMessage(frontSocket, messageBuffer(TAG), true);
    sendMessage(frontSocket, messageBuffer(OTHER_COMMAND));

    messageQueue(backSocket);

    assertReply(false);
}

TEST_F(MessageQueueTest, testReply)
{
    EXPECT_CALL(*handler, doHandle(_, IDENTITY, ElementsAre(), _))
        .WillOnce(Respond(REPLY_SUCCESS, PARAM1, PARAM2));
    sendMessage(frontSocket, messageBuffer(TAG), true);
    sendMessage(frontSocket, messageBuffer(COMMAND));

    messageQueue(backSocket);

    assertReply(true, true);
    auto message = Message {};
    recvMessage(frontSocket, message);
    EXPECT_EQ(PARAM1, messageView(message));
    EXPECT_TRUE(message.more());
    recvMessage(frontSocket, message);
    EXPECT_EQ(PARAM2, messageView(message));
    EXPECT_FALSE(message.more());
}

TEST_F(MessageQueueTest, testWhenBackSocketIsNotRouterIdentityIsEmpty)
{
    unbindSocket(backSocket, ENDPOINT);
    disconnectSocket(frontSocket, ENDPOINT);
    Socket repSocket {context, SocketType::rep};
    bindSocket(repSocket, ENDPOINT);
    connectSocket(frontSocket, ENDPOINT);

    EXPECT_CALL(*handler, doHandle(_, Identity {}, ElementsAre(), _))
        .WillOnce(Respond(REPLY_SUCCESS));

    sendMessage(frontSocket, messageBuffer(TAG), true);
    sendMessage(frontSocket, messageBuffer(COMMAND));
    messageQueue(repSocket);
    assertReply(true);
}

TEST_F(MessageQueueTest, testTrySetNewHandlerForNewCommand)
{
    const auto other_handler = std::make_shared<MockMessageHandler>();
    EXPECT_TRUE(messageQueue.trySetHandler(OTHER_COMMAND, other_handler));
    EXPECT_CALL(*other_handler, doHandle(_, IDENTITY, _, _))
        .WillOnce(Respond(REPLY_SUCCESS));
    sendMessage(frontSocket, messageBuffer(TAG), true);
    sendMessage(frontSocket, messageBuffer(OTHER_COMMAND));
    messageQueue(backSocket);
    assertReply(true);
}

TEST_F(MessageQueueTest, testTrySetNewHandlerForOldCommand)
{
    EXPECT_FALSE(
        messageQueue.trySetHandler(
            COMMAND, std::make_shared<MockMessageHandler>()));
    EXPECT_CALL(*handler, doHandle(_, IDENTITY, _, _))
        .WillOnce(Respond(REPLY_SUCCESS));
    sendMessage(frontSocket, messageBuffer(TAG), true);
    sendMessage(frontSocket, messageBuffer(COMMAND));
    messageQueue(backSocket);
    assertReply(true);
}
