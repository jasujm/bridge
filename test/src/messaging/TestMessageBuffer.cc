#include "messaging/MessageBuffer.hh"
#include "messaging/Sockets.hh"

#include <gtest/gtest.h>

#include <array>
#include <iostream>
#include <string>
#include <utility>

namespace {
using namespace std::string_view_literals;
constexpr auto ENDPOINT = "inproc://example"sv;
constexpr auto MESSAGE = "message"sv;
constexpr auto NUMBER = 123;
constexpr auto WHOLE_MESSAGE = "message 123\n"sv;
}

using namespace Bridge::Messaging;

class MessageBufferTest : public testing::Test {
protected:
    virtual void SetUp()
    {
        bindSocket(*backSocket, ENDPOINT);
        connectSocket(*frontSocket, ENDPOINT);
    }

    MessageContext context;
    SharedSocket frontSocket {makeSharedSocket(context, SocketType::pair)};
    SharedSocket backSocket {makeSharedSocket(context, SocketType::pair)};
};

TEST_F(MessageBufferTest, testOutputMessage)
{
    SynchronousMessageOStream out {std::move(frontSocket)};
    out << MESSAGE << " " << NUMBER << std::endl;

    auto message = Message {};
    recvMessage(*backSocket, message);
    EXPECT_EQ(
        WHOLE_MESSAGE, std::string_view(message.data<char>(), message.size()));
    EXPECT_FALSE(message.more());
}

TEST_F(MessageBufferTest, testFlushEmptyOutputShouldNotSendMessage)
{
    SynchronousMessageOStream out {std::move(frontSocket)};
    out.flush();

    auto pollitems = std::array {
        Pollitem { backSocket->handle(), 0, ZMQ_POLLIN, 0 },
    };
    pollSockets(pollitems, 0);
    EXPECT_FALSE(pollitems[0].revents & ZMQ_POLLIN);
}

TEST_F(MessageBufferTest, testInputMessage)
{
    sendMessage(*frontSocket, messageBuffer(WHOLE_MESSAGE));

    SynchronousMessageIStream in {std::move(backSocket)};

    {
        std::string message {};
        in >> message;
        EXPECT_EQ(MESSAGE, message);
    }

    {
        int num {};
        in >> num;
        EXPECT_EQ(NUMBER, num);
    }
}
