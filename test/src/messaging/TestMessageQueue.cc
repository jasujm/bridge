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

using testing::_;
using testing::ElementsAre;
using testing::Return;

using namespace Bridge::Messaging;

namespace {
const auto ENDPOINT = std::string {"inproc://testing"};
const auto COMMAND = std::string {"command"};
const auto TERMINATE = std::string {"terminate"};
}

class MessageQueueTest : public testing::Test {
protected:

    virtual void SetUp()
    {
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
    const auto p1 = std::string {"p1"};
    const auto p2 = std::string {"p2"};

    EXPECT_CALL(*handlers.at(COMMAND), doHandle(ElementsAre(p1, p2)))
        .WillOnce(Return(true));
    sendMessage(socket, COMMAND, true);
    sendMessage(socket, p1, true);
    sendMessage(socket, p2);

    const auto reply = recvMessage(socket);
    ASSERT_EQ(std::make_pair(MessageQueue::REPLY_SUCCESS, false), reply);
}

TEST_F(MessageQueueTest, testValidCommandInvokesCorrectHandlerFailure)
{
    const auto p1 = std::string {"p1"};
    const auto p2 = std::string {"p2"};

    EXPECT_CALL(*handlers.at(COMMAND), doHandle(ElementsAre(p1, p2)))
        .WillOnce(Return(false));
    sendMessage(socket, COMMAND, true);
    sendMessage(socket, p1, true);
    sendMessage(socket, p2);

    const auto reply = recvMessage(socket);
    ASSERT_EQ(std::make_pair(MessageQueue::REPLY_FAILURE, false), reply);
}

TEST_F(MessageQueueTest, testInvalidCommandReturnsError)
{
    EXPECT_CALL(*handlers.at(COMMAND), doHandle(_)).Times(0);
    sendMessage(socket, "invalid");

    const auto reply = recvMessage(socket);
    EXPECT_EQ(std::make_pair(MessageQueue::REPLY_FAILURE, false), reply);
}
