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
}

class MessageQueueTest : public testing::Test {
protected:
    virtual void SetUp()
    {
        backSocket.bind(ENDPOINT);
        frontSocket.setsockopt(ZMQ_IDENTITY, IDENTITY.c_str(), IDENTITY.size());
        frontSocket.connect(ENDPOINT);
    }

    void assertReply(
        bool success, boost::optional<std::string> command, bool more = false)
    {
        const auto status = recvMessage(frontSocket);
        EXPECT_EQ(success, isSuccessful(getStatusCode(status.first)));
        if (command) {
            ASSERT_TRUE(status.second);
            EXPECT_EQ(std::make_pair(*command, more), recvMessage(frontSocket));
        } else {
            ASSERT_FALSE(status.second);
        }
    }

    zmq::context_t context;
    zmq::socket_t frontSocket {context, zmq::socket_type::req};
    zmq::socket_t backSocket {context, zmq::socket_type::router};
    std::map<std::string, std::shared_ptr<MockMessageHandler>> handlers {
        {COMMAND, std::make_shared<MockMessageHandler>()}};
    MessageQueue messageQueue {
        MessageQueue::HandlerMap(handlers.begin(), handlers.end())};
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

    messageQueue(backSocket);
    assertReply(true, COMMAND);
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

    messageQueue(backSocket);
    assertReply(false, COMMAND);
}

TEST_F(MessageQueueTest, testInvalidCommandReturnsError)
{
    EXPECT_CALL(*handlers.at(COMMAND), doHandle(_, _, _)).Times(0);
    sendMessage(frontSocket, "invalid");

    messageQueue(backSocket);

    assertReply(false, boost::none);
}

TEST_F(MessageQueueTest, testReply)
{
    const auto outputs = {"output1"s, "output2"s};

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
        std::make_pair(outputs.begin()[0], true), recvMessage(frontSocket));
    EXPECT_EQ(
        std::make_pair(outputs.begin()[1], false), recvMessage(frontSocket));
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
