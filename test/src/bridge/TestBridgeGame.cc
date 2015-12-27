#include "bridge/Call.hh"
#include "bridge/CardType.hh"
#include "bridge/Bid.hh"
#include "bridge/GameState.hh"
#include "MockBridgeGame.hh"

#include <gtest/gtest.h>

using testing::Return;

class BridgeGameTest : public testing::Test
{
protected:
    Bridge::MockBridgeGame game;
};

TEST_F(BridgeGameTest, testCall)
{
    const Bridge::Call call {Bridge::Bid {1, Bridge::Strain::CLUBS}};
    EXPECT_CALL(game, handleCall(call));
    game.call(call);
}

TEST_F(BridgeGameTest, testPlay)
{
    constexpr Bridge::CardType card {Bridge::Rank::ACE, Bridge::Suit::SPADES};
    EXPECT_CALL(game, handlePlay(card));
    game.play(card);
}

TEST_F(BridgeGameTest, testGetGameState)
{
    // Just an arbitrary state
    Bridge::GameState state;
    state.stage = Bridge::Stage::BIDDING;
    EXPECT_CALL(game, handleGetState()).WillOnce(Return(state));
    EXPECT_EQ(state, game.getState());
}
