#include "bridge/Hand.hh"
#include "bridge/Position.hh"
#include "main/CardProtocol.hh"
#include "MockCardManager.hh"
#include "MockCardProtocol.hh"
#include "MockMessageHandler.hh"
#include "MockMessageLoopCallback.hh"

#include <gtest/gtest.h>
#include <json.hpp>

#include <algorithm>
#include <optional>
#include <iterator>

using namespace Bridge::Messaging;
using Bridge::Main::CardProtocol;

using testing::Bool;
using testing::Ref;
using testing::Return;

class CardProtocolTest : public testing::TestWithParam<bool> {
protected:
    MessageContext context;
    Bridge::Main::MockCardProtocol protocol;
};

TEST_P(CardProtocolTest, testAcceptPeer)
{
    using namespace Bridge::BlobLiterals;
    using namespace std::string_literals;
    const auto success = GetParam();
    const auto identity = Identity { ""s, "identity"_B };
    const auto positions = CardProtocol::PositionVector {
        Bridge::Position::NORTH, Bridge::Position::SOUTH};
    const auto args = std::make_optional(nlohmann::json {123});
    EXPECT_CALL(
        protocol,
        handleAcceptPeer(identity, positions, args)).WillOnce(Return(success));
    EXPECT_EQ(success, protocol.acceptPeer(identity, positions, args));
}

TEST_F(CardProtocolTest, testInitialize)
{
    EXPECT_CALL(protocol, handleInitialize());
    protocol.initialize();
}

TEST_F(CardProtocolTest, testGetMessageHandlers)
{
    const auto expected_handler = std::make_shared<MockMessageHandler>();
    EXPECT_CALL(protocol, handleGetDealMessageHandler())
        .WillOnce(Return(expected_handler));
    EXPECT_EQ(expected_handler, protocol.getDealMessageHandler());
}

TEST_F(CardProtocolTest, testGetSockets)
{
    const auto socket =
        makeSharedSocket(context, SocketType::pair);
    MockMessageLoopCallback callback;
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

INSTANTIATE_TEST_CASE_P(SamplingBools, CardProtocolTest, Bool());
