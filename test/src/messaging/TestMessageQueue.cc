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

using namespace Bridge::Messaging;

using testing::_;
using testing::ElementsAre;
using testing::Invoke;
using testing::IsEmpty;
using testing::Return;

using namespace std::string_literals;

namespace {
const auto IDENTITY = "identity"s;
const auto ENDPOINT = "inproc://testing"s;
const auto COMMAND = "command"s;
const auto TERMINATE = "terminate"s;
}

class MessageQueueTest : public testing::Test {
protected:
    virtual void SetUp()
    {
        backSocket.bind(ENDPOINT);
        frontSocket.setsockopt(ZMQ_IDENTITY, IDENTITY.c_str(), IDENTITY.size());
        frontSocket.connect(ENDPOINT);
    }

    void assertReply(const std::string& reply)
    {
        ASSERT_EQ(std::make_pair(reply, true), recvMessage(frontSocket));
        ASSERT_EQ(std::make_pair(COMMAND, false), recvMessage(frontSocket));
    }

    zmq::context_t context;
    zmq::socket_t frontSocket {context, zmq::socket_type::req};
    zmq::socket_t backSocket {context, zmq::socket_type::router};
    std::map<std::string, std::shared_ptr<MockMessageHandler>> handlers {
        {COMMAND, std::make_shared<MockMessageHandler>()}};
    MessageQueue messageQueue {
        MessageQueue::HandlerMap(handlers.begin(), handlers.end()),
        TERMINATE};
};

TEST_F(MessageQueueTest, testValidCommandInvokesCorrectHandlerSuccessful)
{
    const auto p1 = "p1"s;
    const auto p2 = "p2"s;

    EXPECT_CALL(
        *handlers.at(COMMAND), doHandle(IDENTITY, ElementsAre(p1, p2), _))
        .WillOnce(Return(true));
    sendMessage(frontSocket, COMMAND, true);
    sendMessage(frontSocket, p1, true);
    sendMessage(frontSocket, p2);

    EXPECT_TRUE(messageQueue(backSocket));
    assertReply(REPLY_SUCCESS);
}

TEST_F(MessageQueueTest, testValidCommandInvokesCorrectHandlerFailure)
{
    const auto p1 = "p1"s;
    const auto p2 = "p2"s;

    EXPECT_CALL(
        *handlers.at(COMMAND), doHandle(IDENTITY, ElementsAre(p1, p2), _))
        .WillOnce(Return(false));
    sendMessage(frontSocket, COMMAND, true);
    sendMessage(frontSocket, p1, true);
    sendMessage(frontSocket, p2);

    EXPECT_TRUE(messageQueue(backSocket));
    assertReply(REPLY_FAILURE);
}

TEST_F(MessageQueueTest, testInvalidCommandReturnsError)
{
    EXPECT_CALL(*handlers.at(COMMAND), doHandle(_, _, _)).Times(0);
    sendMessage(frontSocket, "invalid");

    EXPECT_TRUE(messageQueue(backSocket));

    EXPECT_EQ(
        std::make_pair(REPLY_FAILURE, false),
        recvMessage(frontSocket));
}

TEST_F(MessageQueueTest, testReply)
{
    const auto outputs = {"output1"s, "output2"s};

    EXPECT_CALL(
        *handlers.at(COMMAND), doHandle(IDENTITY, IsEmpty(), _))
        .WillOnce(
            Invoke(
                MockMessageHandler::writeToSink(
                    outputs.begin(), outputs.end())));
    sendMessage(frontSocket, COMMAND, false);

    EXPECT_TRUE(messageQueue(backSocket));

    EXPECT_EQ(
        std::make_pair(REPLY_SUCCESS, true),
        recvMessage(frontSocket));
    ASSERT_EQ(std::make_pair(COMMAND, true), recvMessage(frontSocket));
    EXPECT_EQ(
        std::make_pair(outputs.begin()[0], true), recvMessage(frontSocket));
    EXPECT_EQ(
        std::make_pair(outputs.begin()[1], false), recvMessage(frontSocket));
}

TEST_F(MessageQueueTest, testTerminate)
{
    sendMessage(frontSocket, TERMINATE, false);
    EXPECT_FALSE(messageQueue(backSocket));
    EXPECT_EQ(
        std::make_pair(REPLY_SUCCESS, false),
        recvMessage(frontSocket));
}

TEST_F(MessageQueueTest, testWhenBackSocketIsNotRouterIdentityIsEmpty)
{
    backSocket.unbind(ENDPOINT);
    frontSocket.disconnect(ENDPOINT);
    zmq::socket_t repSocket {context, zmq::socket_type::rep};
    repSocket.bind(ENDPOINT);
    frontSocket.connect(ENDPOINT);

    EXPECT_CALL(*handlers.at(COMMAND), doHandle(IsEmpty(), IsEmpty(), _))
        .WillOnce(Return(true));

    sendMessage(frontSocket, COMMAND, false);
    EXPECT_TRUE(messageQueue(repSocket));
    assertReply(REPLY_SUCCESS);
}
