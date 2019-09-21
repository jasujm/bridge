#include "messaging/MessageBuffer.hh"
#include "messaging/MessageHelper.hh"
#include "messaging/Sockets.hh"

#include <gtest/gtest.h>

#include <array>
#include <iostream>
#include <string>
#include <utility>

namespace {
using namespace std::string_literals;
const auto ENDPOINT = "inproc://example"s;
const auto MESSAGE = "message"s;
const auto NUMBER = 123;
const auto WHOLE_MESSAGE = "message 123\n"s;
}

using namespace Bridge::Messaging;

class MessageBufferTest : public testing::Test {
protected:
    virtual void SetUp()
    {
        backSocket->bind(ENDPOINT);
        frontSocket->connect(ENDPOINT);
    }

    MessageContext context;
    SharedSocket frontSocket {makeSharedSocket(context, SocketType::pair)};
    SharedSocket backSocket {makeSharedSocket(context, SocketType::pair)};
};

TEST_F(MessageBufferTest, testOutputMessage)
{
    SynchronousMessageOStream out {std::move(frontSocket)};
    out << MESSAGE << " " << NUMBER << std::endl;

    const auto message = recvMessage<std::string>(*backSocket);
    EXPECT_EQ(std::make_pair(WHOLE_MESSAGE, false), message);
}

TEST_F(MessageBufferTest, testFlushEmptyOutputShouldNotSendMessage)
{
    SynchronousMessageOStream out {std::move(frontSocket)};
    out.flush();

    auto pollitems = std::array {
        Pollitem { static_cast<void*>(*backSocket), 0, ZMQ_POLLIN, 0 },
    };
    pollSockets(pollitems, 0);
    EXPECT_FALSE(pollitems[0].revents & ZMQ_POLLIN);
}

TEST_F(MessageBufferTest, testInputMessage)
{
    sendMessage(*frontSocket, WHOLE_MESSAGE);

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
