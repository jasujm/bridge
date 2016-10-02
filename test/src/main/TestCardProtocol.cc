#include "bridge/Hand.hh"
#include "main/CardProtocol.hh"
#include "MockCardManager.hh"
#include "MockCardProtocol.hh"
#include "MockMessageHandler.hh"
#include "MockMessageLoopCallback.hh"
#include "TestUtility.hh"

#include <gtest/gtest.h>

#include <algorithm>
#include <iterator>

using Bridge::Main::CardProtocol;

using testing::Ref;
using testing::Return;

class CardProtocolTest : public testing::Test {
protected:
    zmq::context_t context;
    Bridge::Main::MockCardProtocol protocol;
};

TEST_F(CardProtocolTest, testSetAcceptor)
{
    const auto acceptor = std::make_shared<Bridge::Main::MockPeerAcceptor>();
    EXPECT_CALL(protocol, handleSetAcceptor(Bridge::WeaklyPointsTo(acceptor)));
    protocol.setAcceptor(acceptor);
}

TEST_F(CardProtocolTest, testGetMessageHandlers)
{
    CardProtocol::MessageHandlerVector expected_handlers {{
        { "command", std::make_shared<Bridge::Messaging::MockMessageHandler>() }
    }};
    EXPECT_CALL(protocol, handleGetMessageHandlers())
        .WillOnce(Return(expected_handlers));
    const auto actual_handlers = protocol.getMessageHandlers();
    EXPECT_TRUE(
        std::equal(
            expected_handlers.begin(), expected_handlers.end(),
            actual_handlers.begin(), actual_handlers.end()));
}

TEST_F(CardProtocolTest, testGetSockets)
{
    const auto socket =
        std::make_shared<zmq::socket_t>(context, zmq::socket_type::pair);
    Bridge::Messaging::MockMessageLoopCallback callback;
    EXPECT_CALL(callback, call(Ref(*socket)));
    CardProtocol::SocketVector expected_sockets {{
        { socket, [&callback](auto& socket) { callback.call(socket); } }
    }};
    EXPECT_CALL(protocol, handleGetSockets())
        .WillOnce(Return(expected_sockets));
    const auto actual_sockets = protocol.getSockets();
    ASSERT_EQ(1, std::distance(actual_sockets.begin(), actual_sockets.end()));
    const auto pair = *actual_sockets.begin();
    pair.second(*pair.first);
}

TEST_F(CardProtocolTest, testGetCardManager)
{
    const auto card_manager =
        std::make_shared<Bridge::Engine::MockCardManager>();
    EXPECT_CALL(protocol, handleGetCardManager())
        .WillOnce(Return(card_manager));
    EXPECT_EQ(card_manager, protocol.getCardManager());
}
