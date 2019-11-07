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
using Bridge::ByteSpan;
using Bridge::dereference;
using Bridge::CardServer::PeerSocketProxy;

using testing::_;
using testing::Field;
using testing::Return;

using OrderParameter = std::uint8_t;

namespace {

using namespace std::string_view_literals;
using namespace Bridge::BlobLiterals;

constexpr auto SELF_ENDPOINT = "inproc://bridge.test.peersocketproxy.self"sv;
constexpr auto PEER1_ENDPOINT = "inproc://bridge.test.peersocketproxy.peer1"sv;
constexpr auto PEER3_ENDPOINT = "inproc://bridge.test.peersocketproxy.peer3"sv;
const auto PEER_IDENTITY = "peer"_B;
const auto MESSAGE = "message"_BS;
constexpr auto ORDER = OrderParameter {1};

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

    template<typename OrderParameter>
    void testIncomingMessageHelper(
        OrderParameter order, std::optional<ByteSpan> message,
        std::optional<ByteSpan> peer1Message,
        std::optional<ByteSpan> peer3Message,
        bool skipEmptyFrame = false)
    {
        auto socket = Socket {context, SocketType::dealer};
        socket.setsockopt(
            ZMQ_IDENTITY, PEER_IDENTITY.data(), PEER_IDENTITY.size());
        connectSocket(socket, SELF_ENDPOINT);
        const auto order_parameter = boost::endian::native_to_big(order);
        if (!skipEmptyFrame) {
            sendEmptyMessage(socket, true);
        }
        const auto order_parameter_buffer =
            messageBuffer(&order_parameter, sizeof(order_parameter));
        sendMessage(socket, order_parameter_buffer, bool(message));
        if (message) {
            sendMessage(socket, messageBuffer(*message));
        }
        pollAndDispatch();
        const auto stream_sockets = proxy.getStreamSockets();
        auto msg = Message {};
        auto recv_result =
            recvMessageNonblocking(dereference(stream_sockets.at(0)), msg);
        EXPECT_EQ(bool(peer1Message), bool(recv_result));
        if (peer1Message) {
            EXPECT_EQ(*peer1Message, messageView(msg));
        }
        recv_result =
            recvMessageNonblocking(dereference(stream_sockets.at(1)), msg);
        EXPECT_EQ(bool(peer3Message), bool(recv_result));
        if (peer3Message) {
            EXPECT_EQ(*peer3Message, messageView(msg));
        }
    }

    void testOutgoingMessageHelper(int peerIndex, std::string_view peerEndpoint)
    {
        auto socket = Socket {context, SocketType::dealer};
        bindSocket(socket, peerEndpoint);
        const auto stream_sockets = proxy.getStreamSockets();
        sendMessage(
            dereference(stream_sockets.at(peerIndex)), messageBuffer(MESSAGE));
        pollAndDispatch();
        auto msg = Message {};
        const auto recv_result = recvMessageNonblocking(socket, msg);
        EXPECT_TRUE(recv_result);
        EXPECT_EQ(0u, msg.size());
        ASSERT_TRUE(msg.more());
        recvMessage(socket, msg);
        auto order_parameter = OrderParameter {};
        ASSERT_EQ(sizeof(OrderParameter), msg.size());
        std::memcpy(&order_parameter, msg.data(), sizeof(OrderParameter));
        EXPECT_EQ(ORDER, boost::endian::big_to_native(order_parameter));
        ASSERT_TRUE(msg.more());
        recvMessage(socket, msg);
        EXPECT_EQ(MESSAGE, messageView(msg));
        EXPECT_FALSE(msg.more());
    }

    MessageContext context;
    testing::NiceMock<MockAuthorizationFunction> authorizer;
    PeerSocketProxy proxy {
        [this]() {
            auto peerServerSocket = Socket {context, SocketType::router};
            bindSocket(peerServerSocket, SELF_ENDPOINT);
            auto peerClientSockets = std::vector<Socket> {};
            peerClientSockets.emplace_back(context, SocketType::dealer);
            connectSocket(peerClientSockets.back(), PEER1_ENDPOINT);
            peerClientSockets.emplace_back(context, SocketType::dealer);
            connectSocket(peerClientSockets.back(), PEER3_ENDPOINT);
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

TEST_F(PeerSocketProxyTest, testMessageMissingEmptyFrame)
{
    testIncomingMessageHelper(
        OrderParameter {0}, MESSAGE, std::nullopt, std::nullopt, true);
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
