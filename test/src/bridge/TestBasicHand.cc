#include "bridge/BasicHand.hh"
#include "bridge/BridgeConstants.hh"
#include "MockCard.hh"
#include "MockHand.hh"
#include "Utility.hh"

#include <gtest/gtest.h>

#include <array>

using Bridge::BasicHand;
using Bridge::N_CARDS_PER_PLAYER;

using testing::_;
using testing::ElementsAreArray;
using testing::Return;
using testing::Values;

class BasicHandTest : public testing::TestWithParam<std::size_t> {
protected:
    virtual void SetUp()
    {
        hand.subscribe(observer);
    }

    std::array<testing::NiceMock<Bridge::MockCard>, N_CARDS_PER_PLAYER> cards;
    BasicHand hand {cards.begin(), cards.end()};
    std::shared_ptr<Bridge::MockCardRevealStateObserver> observer {
        std::make_shared<Bridge::MockCardRevealStateObserver>()};
};

TEST_P(BasicHandTest, testRequestReveal)
{
    const auto range = Bridge::to(std::size_t {GetParam()});
    EXPECT_CALL(
        *observer,
        handleNotify(
            BasicHand::CardRevealState::REQUESTED, ElementsAreArray(range)));
    hand.requestReveal(range.begin(), range.end());
}

TEST_P(BasicHandTest, testRevealKnownCards)
{
    const auto range = Bridge::to(std::size_t {GetParam()});
    EXPECT_CALL(
        *observer,
        handleNotify(
            BasicHand::CardRevealState::COMPLETED, ElementsAreArray(range)));
    for (auto& card : cards) {
        ON_CALL(card, handleIsKnown()).WillByDefault(Return(true));
    }
    EXPECT_TRUE(hand.reveal(range.begin(), range.end()));
}

TEST_P(BasicHandTest, testRevealPlayedCards)
{
    const auto range = Bridge::to(std::size_t {GetParam()});
    EXPECT_CALL(
        *observer,
        handleNotify(
            BasicHand::CardRevealState::COMPLETED, ElementsAreArray(range)));
    for (const auto n : Bridge::to(hand.getNumberOfCards())) {
        hand.markPlayed(n);
    }
    EXPECT_TRUE(hand.reveal(range.begin(), range.end()));
}

TEST_P(BasicHandTest, testFailedReveal)
{
    const auto range = Bridge::to(std::size_t {GetParam()});
    EXPECT_CALL(*observer, handleNotify(_, _)).Times(0);
    for (auto& card : cards) {
        ON_CALL(card, handleIsKnown()).WillByDefault(Return(false));
    }
    EXPECT_FALSE(hand.reveal(range.begin(), range.end()));
}

INSTANTIATE_TEST_CASE_P(SamplingCards, BasicHandTest, Values(1, 2, 3, 5, 8));
