#include "messaging/FunctionMessageHandler.hh"
#include "messaging/MessageHandler.hh"
#include "messaging/MessageQueue.hh"
#include "messaging/MessageUtility.hh"
#include "messaging/Replies.hh"
#include "MockMessageHandler.hh"

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <zmq.hpp>

#include <map>
#include <string>
#include <utility>

using Bridge::asBytes;
using Bridge::Blob;
using namespace Bridge::Messaging;

using testing::_;
using testing::ElementsAre;
using testing::Invoke;
using testing::IsEmpty;
using testing::Return;

using namespace std::string_literals;

namespace {
const auto IDENTITY = Identity { std::byte {123}, std::byte {32} };
const auto PARAM1 = Blob { std::byte {123} };
const auto PARAM2 = Blob { std::byte {32} };
const auto ENDPOINT = "inproc://testing"s;
const auto COMMAND = Blob { std::byte {65}, std::byte {66} };
const auto OTHER_COMMAND = Blob { std::byte {67}, std::byte {68} };
}

class MessageQueueTest : public testing::Test {
protected:
    virtual void SetUp()
    {
        backSocket.bind(ENDPOINT);
        frontSocket.setsockopt(ZMQ_IDENTITY, IDENTITY.data(), IDENTITY.size());
        frontSocket.connect(ENDPOINT);
    }

    void assertReply(
        bool success, std::optional<Blob> command, bool more = false)
    {
        const auto status = recvMessage<Blob>(frontSocket);
        EXPECT_EQ(success, isSuccessful(getStatusCode(status.first)));
        if (command) {
            ASSERT_TRUE(status.second);
            EXPECT_EQ(
                std::make_pair(*command, more), recvMessage<Blob>(frontSocket));
        } else {
            ASSERT_FALSE(status.second);
        }
    }

    zmq::context_t context;
    zmq::socket_t frontSocket {context, zmq::socket_type::req};
    zmq::socket_t backSocket {context, zmq::socket_type::router};
    std::map<Blob, std::shared_ptr<MockMessageHandler>> handlers {
        {COMMAND, std::make_shared<MockMessageHandler>()}};
    MessageQueue messageQueue {
        MessageQueue::HandlerMap(handlers.begin(), handlers.end())};
};

TEST_F(MessageQueueTest, testValidCommandInvokesCorrectHandlerSuccessful)
{
    EXPECT_CALL(
        *handlers.at(COMMAND),
        doHandle(IDENTITY, ElementsAre(asBytes(PARAM1), asBytes(PARAM2)), _))
        .WillOnce(Return(true));
    sendMessage(frontSocket, COMMAND, true);
    sendMessage(frontSocket, PARAM1, true);
    sendMessage(frontSocket, PARAM2);

    messageQueue(backSocket);
    assertReply(true, COMMAND);
}

TEST_F(MessageQueueTest, testValidCommandInvokesCorrectHandlerFailure)
{
    EXPECT_CALL(
        *handlers.at(COMMAND),
        doHandle(IDENTITY, ElementsAre(asBytes(PARAM1), asBytes(PARAM2)), _))
        .WillOnce(Return(false));
    sendMessage(frontSocket, COMMAND, true);
    sendMessage(frontSocket, PARAM1, true);
    sendMessage(frontSocket, PARAM2);

    messageQueue(backSocket);
    assertReply(false, COMMAND);
}

TEST_F(MessageQueueTest, testInvalidCommandReturnsError)
{
    EXPECT_CALL(*handlers.at(COMMAND), doHandle(_, _, _)).Times(0);
    sendMessage(frontSocket, OTHER_COMMAND);

    messageQueue(backSocket);

    assertReply(false, OTHER_COMMAND);
}

TEST_F(MessageQueueTest, testReply)
{
    const auto outputs = { PARAM1, PARAM2 };

    EXPECT_CALL(
        *handlers.at(COMMAND), doHandle(IDENTITY, ElementsAre(), _))
        .WillOnce(
            Invoke(
                MockMessageHandler::writeToSink(
                    outputs.begin(), outputs.end())));
    sendMessage(frontSocket, COMMAND, false);

    messageQueue(backSocket);

    assertReply(true, COMMAND, true);
    EXPECT_EQ(
        std::make_pair(outputs.begin()[0], true),
        recvMessage<Blob>(frontSocket));
    EXPECT_EQ(
        std::make_pair(outputs.begin()[1], false),
        recvMessage<Blob>(frontSocket));
}

TEST_F(MessageQueueTest, testWhenBackSocketIsNotRouterIdentityIsEmpty)
{
    backSocket.unbind(ENDPOINT);
    frontSocket.disconnect(ENDPOINT);
    zmq::socket_t repSocket {context, zmq::socket_type::rep};
    repSocket.bind(ENDPOINT);
    frontSocket.connect(ENDPOINT);

    EXPECT_CALL(*handlers.at(COMMAND), doHandle(IsEmpty(), ElementsAre(), _))
        .WillOnce(Return(true));

    sendMessage(frontSocket, COMMAND, false);
    messageQueue(repSocket);
    assertReply(true, COMMAND);
}

TEST_F(MessageQueueTest, testTrySetNewHandlerForNewCommand)
{
    const auto other_handler = std::make_shared<MockMessageHandler>();
    EXPECT_TRUE(messageQueue.trySetHandler(OTHER_COMMAND, other_handler));
    EXPECT_CALL(*other_handler, doHandle(IDENTITY, _, _))
        .WillOnce(Return(true));
    sendMessage(frontSocket, OTHER_COMMAND);
    messageQueue(backSocket);
    assertReply(true, OTHER_COMMAND);
}

TEST_F(MessageQueueTest, testTrySetNewHandlerForOldCommand)
{
    EXPECT_FALSE(
        messageQueue.trySetHandler(
            COMMAND, std::make_shared<MockMessageHandler>()));
    EXPECT_CALL(*handlers.at(COMMAND), doHandle(IDENTITY, _, _))
        .WillOnce(Return(true));
    sendMessage(frontSocket, COMMAND);
    messageQueue(backSocket);
    assertReply(true, COMMAND);
}

TEST_F(MessageQueueTest, testTrySetOldHandlerForOldCommand)
{
    EXPECT_TRUE(messageQueue.trySetHandler(COMMAND, handlers.at(COMMAND)));
}
