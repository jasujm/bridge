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

using Bridge::Messaging::MessageHandler;
using Bridge::Messaging::MessageQueue;
using Bridge::Messaging::MockMessageHandler;
using Bridge::Messaging::recvMessage;
using Bridge::Messaging::sendMessage;

namespace {
const auto ENDPOINT = std::string {"inproc://testing"};
const auto COMMAND = std::string {"command"};
}

class MessageQueueTest : public testing::Test {
protected:

    virtual void SetUp()
    {
        socket.connect(ENDPOINT);
    }

    virtual void TearDown()
    {
        sendMessage(socket, MessageQueue::MESSAGE_TERMINATE);
        const auto reply = recvMessage(socket);
        EXPECT_FALSE(reply.second);
        messageThread.join();
    }

    zmq::context_t context;
    std::map<std::string, std::shared_ptr<MockMessageHandler>> handlers {
        {COMMAND, std::make_shared<MockMessageHandler>()}};
    MessageQueue messageQueue {
        handlers.begin(), handlers.end(), context, ENDPOINT};
    std::thread messageThread {[this]() { messageQueue.run(); }};
    zmq::socket_t socket {context, zmq::socket_type::req};
};

TEST_F(MessageQueueTest, testValidCommandInvokesCorrectHandler)
{
    const auto p1 = std::string {"p1"};
    const auto p2 = std::string {"p2"};
    const auto r1 = std::string {"r1"};
    const auto r2 = std::string {"r2"};

    EXPECT_CALL(*handlers.at(COMMAND), doHandle(ElementsAre(p1, p2)))
        .WillOnce(Return(MessageHandler::ReturnValue {r1, r2}));
    sendMessage(socket, COMMAND, true);
    sendMessage(socket, p1, true);
    sendMessage(socket, p2);

    auto reply = recvMessage(socket);
    ASSERT_EQ(std::make_pair(MessageQueue::REPLY_SUCCESS, true), reply);
    reply = recvMessage(socket);
    ASSERT_EQ(std::make_pair(r1, true), reply);
    reply = recvMessage(socket);
    ASSERT_EQ(std::make_pair(r2, false), reply);
}

TEST_F(MessageQueueTest, testInvalidCommandReturnsError)
{
    EXPECT_CALL(*handlers.at(COMMAND), doHandle(_)).Times(0);
    sendMessage(socket, "invalid");

    const auto reply = recvMessage(socket);
    EXPECT_EQ(std::make_pair(MessageQueue::REPLY_FAILURE, false), reply);
}
