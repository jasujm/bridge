#include "bridge/Call.hh"
#include "bridge/CardType.hh"
#include "bridge/Bid.hh"
#include "MockBridgeController.hh"

#include <gtest/gtest.h>

using testing::Return;

class BridgeControllerTest : public testing::Test
{
protected:
    Bridge::MockBridgeController game;
};

TEST_F(BridgeControllerTest, testCall)
{
    const Bridge::Call call {Bridge::Bid {1, Bridge::Strain::CLUBS}};
    EXPECT_CALL(game, handleCall(call));
    game.call(call);
}

TEST_F(BridgeControllerTest, testPlay)
{
    constexpr Bridge::CardType card {Bridge::Rank::ACE, Bridge::Suit::SPADES};
    EXPECT_CALL(game, handlePlay(card));
    game.play(card);
}
