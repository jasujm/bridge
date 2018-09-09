#include "messaging/MessageBuffer.hh"
#include "messaging/MessageHelper.hh"

#include <gtest/gtest.h>
#include <zmq.hpp>

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

using Bridge::Messaging::sendMessage;
using Bridge::Messaging::recvMessage;
using Bridge::Messaging::MessageBuffer;

class MessageBufferTest : public testing::Test {
protected:
    virtual void SetUp()
    {
        backSocket->bind(ENDPOINT);
        frontSocket->connect(ENDPOINT);
    }

    zmq::context_t context;
    std::shared_ptr<zmq::socket_t> frontSocket {
        std::make_shared<zmq::socket_t>(context, zmq::socket_type::pair)};
    std::shared_ptr<zmq::socket_t> backSocket {
        std::make_shared<zmq::socket_t>(context, zmq::socket_type::pair)};
};

TEST_F(MessageBufferTest, testOutputMessage)
{
    MessageBuffer buffer {std::move(frontSocket)};
    std::ostream out {&buffer};
    out << MESSAGE << " " << NUMBER << std::endl;

    const auto message = recvMessage<std::string>(*backSocket);
    EXPECT_EQ(std::make_pair(WHOLE_MESSAGE, false), message);
}

TEST_F(MessageBufferTest, testFlushEmptyOutputShouldNotSendMessage)
{
    MessageBuffer buffer {std::move(frontSocket)};
    std::ostream out {&buffer};
    out.flush();

    std::array<zmq::pollitem_t, 1> pollitems {{
        {static_cast<void*>(*backSocket), 0, ZMQ_POLLIN, 0},
    }};
    EXPECT_EQ(0, zmq::poll(pollitems.data(), 1, 0));
}

TEST_F(MessageBufferTest, testInputMessage)
{
    sendMessage(*frontSocket, WHOLE_MESSAGE);

    MessageBuffer buffer {std::move(backSocket)};
    std::istream in {&buffer};

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
