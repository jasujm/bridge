#include "messaging/MessageUtility.hh"
#include "csmain/PeerSocketProxy.hh"
#include "Blob.hh"
#include "Utility.hh"

#include <boost/endian/conversion.hpp>
#include <gtest/gtest.h>

#include <cstring>
#include <optional>
#include <string>

using Bridge::asBytes;
using Bridge::dereference;
using Bridge::CardServer::PeerSocketProxy;
using Bridge::Messaging::messageView;

using OrderParameter = PeerSocketProxy::OrderParameter;

namespace {

using namespace std::string_literals;

const auto SELF_ENDPOINT = "inproc://bridge.test.peersocketproxy.self"s;
const auto PEER1_ENDPOINT = "inproc://bridge.test.peersocketproxy.peer1"s;
const auto PEER3_ENDPOINT = "inproc://bridge.test.peersocketproxy.peer3"s;
const auto MESSAGE = "message"s;
const auto ORDER = OrderParameter {1};

}

class PeerSocketProxyTest : public testing::Test {
protected:

    void pollAndDispatch()
    {
        const auto pollables = proxy.getPollables();
        for (auto&& [socket, callback] : pollables) {
            auto& socket_ref = dereference(socket);
            if (socket_ref.getsockopt<int>(ZMQ_EVENTS) & ZMQ_POLLIN) {
                callback(socket_ref);
            }
        }
    }

    void testIncomingMessageHelper(
        auto order, std::optional<std::string> message,
        std::optional<std::string> peer1Message,
        std::optional<std::string> peer3Message)
    {
        auto socket = zmq::socket_t {context, zmq::socket_type::dealer};
        socket.connect(SELF_ENDPOINT);
        const auto order_parameter = boost::endian::native_to_big(order);
        socket.send("", 0, ZMQ_SNDMORE);
        socket.send(
            &order_parameter,
            sizeof(order_parameter), message ? ZMQ_SNDMORE : 0);
        if (message) {
            socket.send(message->data(), message->size());
        }
        pollAndDispatch();
        const auto stream_sockets = proxy.getStreamSockets();
        auto msg = zmq::message_t {};
        auto msg_received = dereference(stream_sockets.at(0))
            .recv(&msg, ZMQ_DONTWAIT);
        EXPECT_EQ(bool(peer1Message), msg_received);
        if (peer1Message) {
            EXPECT_EQ(asBytes(*peer1Message), messageView(msg));
        }
        msg_received = dereference(stream_sockets.at(1))
            .recv(&msg, ZMQ_DONTWAIT);
        EXPECT_EQ(bool(peer3Message), msg_received);
        if (peer3Message) {
            EXPECT_EQ(asBytes(*peer3Message), messageView(msg));
        }
    }

    void testOutgoingMessageHelper(std::size_t peerIndex, const std::string& peerEndpoint)
    {
        auto socket = zmq::socket_t {context, zmq::socket_type::dealer};
        socket.bind(peerEndpoint);
        const auto stream_sockets = proxy.getStreamSockets();
        dereference(stream_sockets.at(peerIndex))
            .send(MESSAGE.data(), MESSAGE.size());
        pollAndDispatch();
        auto msg = zmq::message_t {};
        const auto msg_received = socket.recv(&msg, ZMQ_DONTWAIT);
        EXPECT_TRUE(msg_received);
        EXPECT_EQ(0u, msg.size());
        ASSERT_TRUE(msg.more());
        socket.recv(&msg);
        auto order_parameter = OrderParameter {};
        ASSERT_EQ(sizeof(OrderParameter), msg.size());
        std::memcpy(&order_parameter, msg.data(), sizeof(OrderParameter));
        EXPECT_EQ(ORDER, boost::endian::big_to_native(order_parameter));
        ASSERT_TRUE(msg.more());
        socket.recv(&msg);
        EXPECT_EQ(asBytes(MESSAGE), messageView(msg));
        EXPECT_FALSE(msg.more());
    }

    zmq::context_t context;
    PeerSocketProxy proxy {
        [this]() {
            auto peerServerSocket = zmq::socket_t {
                context, zmq::socket_type::router};
            peerServerSocket.bind(SELF_ENDPOINT);
            auto peerClientSockets = std::vector<zmq::socket_t> {};
            peerClientSockets.emplace_back(context, zmq::socket_type::dealer);
            peerClientSockets.back().connect(PEER1_ENDPOINT);
            peerClientSockets.emplace_back(context, zmq::socket_type::dealer);
            peerClientSockets.back().connect(PEER3_ENDPOINT);
            return PeerSocketProxy {
                context, std::move(peerServerSocket),
                std::move(peerClientSockets), 1};
        }()
    };
};

TEST_F(PeerSocketProxyTest, testStreamSockets)
{
    const auto stream_sockets = proxy.getStreamSockets();
    EXPECT_EQ(2u, stream_sockets.size());
}

TEST_F(PeerSocketProxyTest, testMessageFromPeerLowOrder)
{
    testIncomingMessageHelper(
        OrderParameter {0}, MESSAGE, MESSAGE, std::nullopt);
}

TEST_F(PeerSocketProxyTest, testMessageFromPeerHighOrder)
{
    testIncomingMessageHelper(
        OrderParameter {2}, MESSAGE, std::nullopt, MESSAGE);
}

TEST_F(PeerSocketProxyTest, testNoMessage)
{
    testIncomingMessageHelper(
        OrderParameter {0}, std::nullopt, std::nullopt, std::nullopt);
}

TEST_F(PeerSocketProxyTest, testMessageWithOrderParameterOfSelf)
{
    testIncomingMessageHelper(ORDER, MESSAGE, std::nullopt, std::nullopt);
}

TEST_F(PeerSocketProxyTest, testMessageFromPeerOrderOutOfBounds)
{
    testIncomingMessageHelper(
        OrderParameter {3}, MESSAGE, std::nullopt, std::nullopt);
}

TEST_F(PeerSocketProxyTest, testMessageIncorrectOrderParameter)
{
    // Wrong "order parameter" size
    testIncomingMessageHelper(
        std::uint16_t {0}, MESSAGE, std::nullopt, std::nullopt);
}

TEST_F(PeerSocketProxyTest, testOutgoingMessagePeer1)
{
    testOutgoingMessageHelper(0, PEER1_ENDPOINT);
}

TEST_F(PeerSocketProxyTest, testOutgoingMessagePeer3)
{
    testOutgoingMessageHelper(1, PEER3_ENDPOINT);
}
