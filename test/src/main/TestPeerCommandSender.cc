#include "messaging/MessageHelper.hh"
#include "messaging/Replies.hh"
#include "messaging/Sockets.hh"
#include "main/PeerCommandSender.hh"
#include "MockCallbackScheduler.hh"
#include "MockSerializationPolicy.hh"

#include <boost/range/combine.hpp>
#include <gtest/gtest.h>

#include <array>
#include <stdexcept>
#include <string>

using Bridge::asBytes;
using namespace Bridge::Messaging;
using testing::_;
using testing::SaveArg;

namespace {
using namespace std::string_view_literals;
constexpr auto N_SOCKETS = 2;
constexpr auto DEFAULT = "default"sv;
constexpr auto NEXT = "next"sv;
constexpr auto KEY = "key"sv;
constexpr auto ARG = "arg"sv;
constexpr auto ENDPOINTS = std::array {
    "inproc://endpoint1"sv,
    "inproc://endpoint2"sv,
};

}

class PeerCommandSenderTest : public testing::Test {
protected:
    virtual void SetUp()
    {
        for (auto&& t : boost::combine(ENDPOINTS, frontSockets, backSockets))
        {
            bindSocket(t.get<1>(), t.get<0>());
            t.get<2>() = sender.addPeer(context, t.get<0>());
        }
    }

    void sendCommand(std::string_view command = DEFAULT)
    {
        sender.sendCommand(
            MockSerializationPolicy {},
            command,
            std::make_pair(KEY, ARG));
    }

    void checkMessage(Socket& socket, std::string_view command)
    {
        auto message = Message {};
        recvMessage(socket, message);
        EXPECT_EQ(0u, message.size());
        ASSERT_TRUE(message.more());
        recvMessage(socket, message);
        EXPECT_EQ(asBytes(command), messageView(message));
        ASSERT_TRUE(message.more());
        recvMessage(socket, message);
        EXPECT_EQ(asBytes(command), messageView(message));
        ASSERT_TRUE(message.more());
        recvMessage(socket, message);
        EXPECT_EQ(asBytes(KEY), messageView(message));
        ASSERT_TRUE(message.more());
        recvMessage(socket, message);
        EXPECT_EQ(asBytes(ARG), messageView(message));
        EXPECT_FALSE(message.more());
    }

    void checkReceive(
        bool recv1 = true, bool recv2 = true,
        std::string_view command = DEFAULT)
    {
        const std::array<bool, N_SOCKETS> expect_recv {{recv1, recv2}};
        auto pollitems = std::array {
            Pollitem { frontSockets[0].handle(), 0, ZMQ_POLLIN, 0 },
            Pollitem { frontSockets[1].handle(), 0, ZMQ_POLLIN, 0 },
        };
        pollSockets(pollitems, 0);
        for (auto&& t : boost::combine(expect_recv, pollitems, frontSockets)) {
            const auto recv = t.get<1>().revents & ZMQ_POLLIN;
            EXPECT_EQ(t.get<0>(), recv);
            if (recv) {
                checkMessage(t.get<2>(), command);
            }
        }
    }

    MessageContext context;
    std::array<Socket, N_SOCKETS> frontSockets {
        Socket {context, SocketType::dealer},
        Socket {context, SocketType::dealer},
    };
    std::array<SharedSocket, N_SOCKETS> backSockets;
    std::shared_ptr<MockCallbackScheduler> callbackScheduler {
        std::make_shared<testing::StrictMock<MockCallbackScheduler>>()};
    Bridge::Main::PeerCommandSender sender {callbackScheduler};
};

TEST_F(PeerCommandSenderTest, testSendToAll)
{
    sendCommand();
    checkReceive();
}

TEST_F(PeerCommandSenderTest, testResendOnFailure)
{
    sendCommand();
    checkReceive();
    sendEmptyMessage(frontSockets[0], true);
    sendMessage(frontSockets[0], messageBuffer(REPLY_FAILURE), true);
    sendMessage(frontSockets[0], messageBuffer(DEFAULT));
    auto callback = MockCallbackScheduler::Callback {};
    EXPECT_CALL(*callbackScheduler, handleCallLater(_, _))
        .WillOnce(SaveArg<1>(&callback));
    sender(*backSockets[0]);
    callback();
    checkReceive(true, false);
}

TEST_F(PeerCommandSenderTest, testSendNextCommandWhenAllSucceed)
{
    sendCommand();
    checkReceive();
    sendCommand(NEXT);
    checkReceive(false, false);
    for (auto&& t : boost::combine(frontSockets, backSockets)) {
        sendEmptyMessage(t.get<0>(), true);
        sendMessage(t.get<0>(), messageBuffer(DEFAULT), true);
        sendMessage(t.get<0>(), messageBuffer(REPLY_SUCCESS));
        sender(*t.get<1>());
    }
    checkReceive(true, true, NEXT);
}

TEST_F(PeerCommandSenderTest, testProcessReplyFailsIfNotPeerSocket)
{
    EXPECT_THROW(
        sender(frontSockets.front()),
        std::invalid_argument);
}
