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

using namespace Bridge::Messaging;
using testing::_;
using testing::SaveArg;

namespace {
using namespace std::string_literals;
constexpr auto N_SOCKETS = 2;
const auto DEFAULT = "default"s;
const auto NEXT = "next"s;
const auto KEY = "key"s;
const auto ARG = "arg"s;
}

class PeerCommandSenderTest : public testing::Test {
protected:
    virtual void SetUp()
    {
        for (auto&& t : boost::combine(endpoints, frontSockets, backSockets))
        {
            t.get<1>().bind(t.get<0>());
            t.get<2>() = sender.addPeer(context, t.get<0>());
        }
    }

    void sendCommand(const std::string& command = DEFAULT)
    {
        sender.sendCommand(
            MockSerializationPolicy {},
            command,
            std::make_pair(KEY, ARG));
    }

    void checkMessage(Socket& socket, const std::string& command)
    {
        ASSERT_EQ(std::make_pair(""s, true), recvMessage<std::string>(socket));
        ASSERT_EQ(std::make_pair(command, true), recvMessage<std::string>(socket));
        ASSERT_EQ(std::make_pair(KEY, true), recvMessage<std::string>(socket));
        EXPECT_EQ(std::make_pair(ARG, false), recvMessage<std::string>(socket));
    }

    void checkReceive(
        bool recv1 = true, bool recv2 = true,
        const std::string& command = DEFAULT)
    {
        const std::array<bool, N_SOCKETS> expect_recv {{recv1, recv2}};
        using Pi = Pollitem;
        auto pollitems = std::array {
            Pi { static_cast<void*>(frontSockets[0]), 0, ZMQ_POLLIN, 0 },
            Pi { static_cast<void*>(frontSockets[1]), 0, ZMQ_POLLIN, 0 },
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

    const std::array<std::string, 3> FAILURE_MESSAGE {
        ""s, std::string(4, '\xff'), DEFAULT
    };
    const std::array<std::string, 3> SUCCESS_MESSAGE {
        ""s, std::string(4, '\0'), DEFAULT
    };

    MessageContext context;
    std::array<std::string, N_SOCKETS> endpoints {{
        "inproc://endpoint1",
        "inproc://endpoint2",
    }};
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
    sendMessage(
        frontSockets[0], FAILURE_MESSAGE.begin(), FAILURE_MESSAGE.end());
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
        sendMessage(t.get<0>(), SUCCESS_MESSAGE.begin(), SUCCESS_MESSAGE.end());
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
