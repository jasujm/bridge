#include "bridge/Hand.hh"
#include "main/CardProtocol.hh"
#include "MockCardManager.hh"
#include "MockCardProtocol.hh"
#include "MockMessageHandler.hh"

#include <gtest/gtest.h>

#include <algorithm>
#include <array>

using testing::Return;

class CardProtocolTest : public testing::Test {
protected:
    Bridge::Main::MockCardProtocol protocol;
};

TEST_F(CardProtocolTest, testGetMessageHandlers)
{
    std::array<Bridge::Main::CardProtocol::MessageHandlerRange::value_type, 1>
    expected_handlers {{
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

TEST_F(CardProtocolTest, testGetCardManager)
{
    const auto card_manager =
        std::make_shared<Bridge::Engine::MockCardManager>();
    EXPECT_CALL(protocol, handleGetCardManager())
        .WillOnce(Return(card_manager));
    EXPECT_EQ(card_manager, protocol.getCardManager());
}
