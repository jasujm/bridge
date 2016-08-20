#include "messaging/MessageUtility.hh"
#include "messaging/Replies.hh"
#include "main/PeerCommandSender.hh"
#include "Zip.hh"
#include "MockSerializationPolicy.hh"

#include <gtest/gtest.h>
#include <zmq.hpp>

#include <array>
#include <stdexcept>
#include <string>

using Bridge::Main::PeerCommandSender;
using Bridge::Messaging::MockSerializationPolicy;
using Bridge::Messaging::recvMessage;
using Bridge::Messaging::sendMessage;
using Bridge::zip;

namespace {
using namespace std::string_literals;
constexpr auto N_SOCKETS = 2u;
const auto DEFAULT = "default"s;
const auto NEXT = "next"s;
const auto KEY = "key"s;
const auto ARG = "arg"s;
}

class PeerCommandSenderTest : public testing::Test {
protected:
    virtual void SetUp()
    {
        for (auto&& t : zip(endpoints, frontSockets, backSockets))
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

    void checkMessage(zmq::socket_t& socket, const std::string& command)
    {
        ASSERT_EQ(std::make_pair(""s, true), recvMessage(socket));
        ASSERT_EQ(std::make_pair(command, true), recvMessage(socket));
        ASSERT_EQ(std::make_pair(KEY, true), recvMessage(socket));
        EXPECT_EQ(std::make_pair(ARG, false), recvMessage(socket));
    }

    void checkReceive(
        bool recv1 = true, bool recv2 = true,
        const std::string& command = DEFAULT)
    {
        const std::array<bool, N_SOCKETS> expect_recv {{recv1, recv2}};
        std::array<zmq::pollitem_t, N_SOCKETS> pollitems {{
            {static_cast<void*>(frontSockets[0]), 0, ZMQ_POLLIN, 0},
            {static_cast<void*>(frontSockets[1]), 0, ZMQ_POLLIN, 0},
        }};
        static_cast<void>(zmq::poll(pollitems.data(), N_SOCKETS, 0));
        for (auto&& t : zip(expect_recv, pollitems, frontSockets)) {
            const auto recv = t.get<1>().revents & ZMQ_POLLIN;
            EXPECT_EQ(t.get<0>(), recv);
            if (recv) {
                checkMessage(t.get<2>(), command);
            }
        }
    }

    const std::array<std::string, 3u> FAILURE_MESSAGE {{
        ""s, Bridge::Messaging::REPLY_FAILURE, DEFAULT}};
    const std::array<std::string, 3u> SUCCESS_MESSAGE {{
        ""s, Bridge::Messaging::REPLY_SUCCESS, DEFAULT}};

    zmq::context_t context;
    std::array<std::string, N_SOCKETS> endpoints {{
        "inproc://endpoint1",
        "inproc://endpoint2",
    }};
    std::array<zmq::socket_t, N_SOCKETS> frontSockets {{
        {context, zmq::socket_type::dealer},
        {context, zmq::socket_type::dealer},
    }};
    std::array<std::shared_ptr<zmq::socket_t>, N_SOCKETS> backSockets;
    PeerCommandSender sender;
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
    sender.processReply(*backSockets[0]);
    checkReceive(true, false);
}

TEST_F(PeerCommandSenderTest, testSendNextCommandWhenAllSucceed)
{
    sendCommand();
    checkReceive();
    sendCommand(NEXT);
    checkReceive(false, false);
    for (auto&& t : zip(frontSockets, backSockets)) {
        sendMessage(t.get<0>(), SUCCESS_MESSAGE.begin(), SUCCESS_MESSAGE.end());
        sender.processReply(*t.get<1>());
    }
    checkReceive(true, true, NEXT);
}

TEST_F(PeerCommandSenderTest, testProcessReplyFailsIfNotPeerSocket)
{
    EXPECT_THROW(
        sender.processReply(frontSockets.front()),
        std::invalid_argument);
}
