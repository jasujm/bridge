#include "messaging/MessageQueue.hh"
#include "messaging/MessageUtility.hh"

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <zmq.hpp>

#include <string>
#include <thread>
#include <utility>

using testing::_;
using testing::ElementsAre;
using testing::Return;

using Bridge::Messaging::MessageQueue;
using Bridge::Messaging::recvMessage;
using Bridge::Messaging::sendMessage;

namespace {

class MockHandler {
public:
    MOCK_METHOD1(
        invoke,
        std::pair<std::string, MessageQueue::HandlerParameters>(
            const MessageQueue::HandlerParameters&));
};

const auto ADDRESS = std::string {"inproc://testing"};
const auto COMMAND = std::string {"command"};
const auto REPLY = std::string {"reply"};

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
        const auto reply = recvMessage(socket);
        EXPECT_FALSE(reply.second);
        messageThread.join();
    }

    zmq::context_t context;
    MockHandler handler;
    std::vector<std::pair<std::string, MessageQueue::Handler>> handlers {
        std::make_pair(
            COMMAND,
            [this](const auto& p)
            {
                return handler.invoke(p);
            }),
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
    const auto p1 = std::string {"p1"};
    const auto p2 = std::string {"p2"};
    const auto r1 = std::string {"r1"};
    const auto r2 = std::string {"r2"};

    EXPECT_CALL(handler, invoke(ElementsAre(p1, p2))).WillOnce(
        Return(std::make_pair(REPLY, std::vector<std::string> {r1, r2})));
    sendMessage(socket, COMMAND, true);
    sendMessage(socket, p1, true);
    sendMessage(socket, p2);

    auto reply = recvMessage(socket);
    ASSERT_EQ(std::make_pair(REPLY, true), reply);
    reply = recvMessage(socket);
    ASSERT_EQ(std::make_pair(r1, true), reply);
    reply = recvMessage(socket);
    ASSERT_EQ(std::make_pair(r2, false), reply);
}

TEST_F(MessageQueueTest, testInvalidCommandReturnsError)
{
    EXPECT_CALL(handler, invoke(_)).Times(0);
    sendMessage(socket, "invalid");

    const auto reply = recvMessage(socket);
    EXPECT_EQ(std::make_pair(MessageQueue::MESSAGE_ERROR, false), reply);
}
