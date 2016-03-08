#include "messaging/FunctionMessageHandler.hh"
#include "messaging/MessageHandler.hh"
#include "messaging/MessageQueue.hh"
#include "messaging/MessageUtility.hh"
#include "MockMessageHandler.hh"

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <zmq.hpp>

#include <map>
#include <string>
#include <thread>
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
        socket.setsockopt(ZMQ_IDENTITY, IDENTITY.c_str(), IDENTITY.size());
        socket.connect(ENDPOINT);
    }

    virtual void TearDown()
    {
        sendMessage(socket, TERMINATE);
        const auto reply = recvMessage(socket);
        EXPECT_FALSE(reply.second);
        messageThread.join();
    }

    zmq::context_t context;
    std::map<std::string, std::shared_ptr<MockMessageHandler>> handlers {
        {COMMAND, std::make_shared<MockMessageHandler>()}};
    MessageQueue messageQueue {
        MessageQueue::HandlerMap(handlers.begin(), handlers.end()),
        context, ENDPOINT, TERMINATE};
    std::thread messageThread {[this]() { messageQueue.run(); }};
    zmq::socket_t socket {context, zmq::socket_type::req};
};

TEST_F(MessageQueueTest, testValidCommandInvokesCorrectHandlerSuccessful)
{
    const auto p1 = "p1"s;
    const auto p2 = "p2"s;

    EXPECT_CALL(
        *handlers.at(COMMAND), doHandle(IDENTITY, ElementsAre(p1, p2), _))
        .WillOnce(Return(true));
    sendMessage(socket, COMMAND, true);
    sendMessage(socket, p1, true);
    sendMessage(socket, p2);

    const auto reply = recvMessage(socket);
    ASSERT_EQ(std::make_pair(MessageQueue::REPLY_SUCCESS, false), reply);
}

TEST_F(MessageQueueTest, testValidCommandInvokesCorrectHandlerFailure)
{
    const auto p1 = "p1"s;
    const auto p2 = "p2"s;

    EXPECT_CALL(
        *handlers.at(COMMAND), doHandle(IDENTITY, ElementsAre(p1, p2), _))
        .WillOnce(Return(false));
    sendMessage(socket, COMMAND, true);
    sendMessage(socket, p1, true);
    sendMessage(socket, p2);

    const auto reply = recvMessage(socket);
    ASSERT_EQ(std::make_pair(MessageQueue::REPLY_FAILURE, false), reply);
}

TEST_F(MessageQueueTest, testInvalidCommandReturnsError)
{
    EXPECT_CALL(*handlers.at(COMMAND), doHandle(_, _, _)).Times(0);
    sendMessage(socket, "invalid");

    const auto reply = recvMessage(socket);
    EXPECT_EQ(std::make_pair(MessageQueue::REPLY_FAILURE, false), reply);
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
    sendMessage(socket, COMMAND, false);

    EXPECT_EQ(
        std::make_pair(MessageQueue::REPLY_SUCCESS, true), recvMessage(socket));
    EXPECT_EQ(std::make_pair(outputs.begin()[0], true), recvMessage(socket));
    EXPECT_EQ(std::make_pair(outputs.begin()[1], false), recvMessage(socket));
}
