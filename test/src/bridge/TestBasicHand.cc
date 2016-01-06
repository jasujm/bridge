#include "bridge/BasicHand.hh"
#include "bridge/BridgeConstants.hh"
#include "MockCard.hh"

#include <gtest/gtest.h>

#include <array>

using Bridge::N_CARDS_PER_PLAYER;

using testing::Values;

class BasicHandTest : public testing::TestWithParam<std::size_t> {
protected:
    std::array<testing::NiceMock<Bridge::MockCard>, N_CARDS_PER_PLAYER> cards;
    Bridge::BasicHand hand {cards.begin(), cards.end()};
};

TEST_F(BasicHandTest, testNumberOfCards)
{
    EXPECT_EQ(N_CARDS_PER_PLAYER, hand.getNumberOfCards());
}

TEST_P(BasicHandTest, testUnplayedCards)
{
    const auto n = GetParam();
    EXPECT_EQ(&cards[n], hand.getCard(n));
}

TEST_P(BasicHandTest, testPlayedCards)
{
    const auto n = GetParam();
    hand.markPlayed(n);
    EXPECT_FALSE(hand.getCard(n));
}

INSTANTIATE_TEST_CASE_P(SamplingCards, BasicHandTest, Values(1, 2, 3, 5, 8));
