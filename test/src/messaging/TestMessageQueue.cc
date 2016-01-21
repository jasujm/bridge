#include "messaging/MessageQueue.hh"
#include "messaging/MessageUtility.hh"

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <zmq.hpp>

#include <string>
#include <thread>
#include <utility>

using testing::Return;

using Bridge::Messaging::MessageQueue;
using Bridge::Messaging::recvMessage;
using Bridge::Messaging::sendMessage;

namespace {

class MockHandler {
public:
    MOCK_METHOD0(invoke, std::string());
};

const std::string ADDRESS {"inproc://testing"};
const std::string COMMAND {"command"};
const std::string REPLY {"reply"};

}

class MessageQueueTest : public testing::Test {
protected:

    virtual void SetUp()
    {
        socket.connect(ADDRESS);
    }

    virtual void TearDown()
    {
        sendMessage(socket, MessageQueue::MESSAGE_TERMINATE);
        static_cast<void>(recvMessage(socket));
        messageThread.join();
    }

    zmq::context_t context;
    MockHandler handler;
    std::vector<std::pair<std::string, MessageQueue::Handler>> handlers {
        std::make_pair(COMMAND, [this]() { return handler.invoke(); }),
    };
    MessageQueue messageQueue {
        handlers.begin(), handlers.end(), makeSocketForMessageQueue()};
    std::thread messageThread {[this]() { messageQueue.run(); }};
    zmq::socket_t socket {context, zmq::socket_type::req};

private:

    zmq::socket_t makeSocketForMessageQueue()
    {
        auto socket = zmq::socket_t {context, zmq::socket_type::rep};
        socket.bind(ADDRESS);
        return socket;
    }
};

TEST_F(MessageQueueTest, testValidCommandInvokesCorrectHandler)
{
    EXPECT_CALL(handler, invoke()).WillOnce(Return(REPLY));
    sendMessage(socket, COMMAND);

    const auto reply = recvMessage(socket);
    EXPECT_EQ(REPLY, reply);
}

TEST_F(MessageQueueTest, testInvalidCommandReturnsError)
{
    EXPECT_CALL(handler, invoke()).Times(0);
    sendMessage(socket, "invalid");

    const auto reply = recvMessage(socket);
    EXPECT_EQ(MessageQueue::MESSAGE_ERROR, reply);
}
