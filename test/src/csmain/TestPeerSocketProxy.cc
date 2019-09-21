#include "messaging/MessageUtility.hh"
#include "csmain/PeerSocketProxy.hh"
#include "Blob.hh"
#include "Utility.hh"

#include <boost/endian/conversion.hpp>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <cstdint>
#include <cstring>
#include <optional>
#include <string>

using namespace Bridge::Messaging;
using Bridge::asBytes;
using Bridge::dereference;
using Bridge::CardServer::PeerSocketProxy;

using testing::_;
using testing::Field;
using testing::Return;

using OrderParameter = std::uint8_t;

namespace {

using namespace std::string_literals;
using namespace Bridge::BlobLiterals;

const auto SELF_ENDPOINT = "inproc://bridge.test.peersocketproxy.self"s;
const auto PEER1_ENDPOINT = "inproc://bridge.test.peersocketproxy.peer1"s;
const auto PEER3_ENDPOINT = "inproc://bridge.test.peersocketproxy.peer3"s;
const auto PEER_IDENTITY = "peer"_B;
const auto MESSAGE = "message"s;
const auto ORDER = OrderParameter {1};

class MockAuthorizationFunction {
public:
    MOCK_METHOD2(authorize, bool(const Identity&, OrderParameter));
    bool operator()(const Identity& identity, OrderParameter order)
    {
        return authorize(identity, order);
    };
};

}

class PeerSocketProxyTest : public testing::Test {
protected:

    virtual void SetUp()
    {
        ON_CALL(authorizer, authorize(_, _)).WillByDefault(Return(true));
    }

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
        auto socket = Socket {context, SocketType::dealer};
        socket.setsockopt(
            ZMQ_IDENTITY, PEER_IDENTITY.data(), PEER_IDENTITY.size());
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
        auto msg = Message {};
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

    void testOutgoingMessageHelper(int peerIndex, const std::string& peerEndpoint)
    {
        auto socket = Socket {context, SocketType::dealer};
        socket.bind(peerEndpoint);
        const auto stream_sockets = proxy.getStreamSockets();
        dereference(stream_sockets.at(peerIndex))
            .send(MESSAGE.data(), MESSAGE.size());
        pollAndDispatch();
        auto msg = Message {};
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

    MessageContext context;
    testing::NiceMock<MockAuthorizationFunction> authorizer;
    PeerSocketProxy proxy {
        [this]() {
            auto peerServerSocket = Socket {context, SocketType::router};
            peerServerSocket.bind(SELF_ENDPOINT);
            auto peerClientSockets = std::vector<Socket> {};
            peerClientSockets.emplace_back(context, SocketType::dealer);
            peerClientSockets.back().connect(PEER1_ENDPOINT);
            peerClientSockets.emplace_back(context, SocketType::dealer);
            peerClientSockets.back().connect(PEER3_ENDPOINT);
            return PeerSocketProxy {
                context, std::move(peerServerSocket),
                std::move(peerClientSockets), 1, std::ref(authorizer)};
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

TEST_F(PeerSocketProxyTest, testAuthorizedMessage)
{
    EXPECT_CALL(
        authorizer,
        authorize(Field(&Identity::routingId, PEER_IDENTITY), 0))
        .WillOnce(Return(true));
    testIncomingMessageHelper(
        OrderParameter {0}, MESSAGE, MESSAGE, std::nullopt);
}

TEST_F(PeerSocketProxyTest, testUnauthorizedMessage)
{
    EXPECT_CALL(
        authorizer,
        authorize(Field(&Identity::routingId, PEER_IDENTITY), 2))
        .WillOnce(Return(false));
    testIncomingMessageHelper(
        OrderParameter {2}, MESSAGE, std::nullopt, std::nullopt);
}

TEST_F(PeerSocketProxyTest, testOutgoingMessagePeer1)
{
    testOutgoingMessageHelper(0, PEER1_ENDPOINT);
}

TEST_F(PeerSocketProxyTest, testOutgoingMessagePeer3)
{
    testOutgoingMessageHelper(1, PEER3_ENDPOINT);
}
