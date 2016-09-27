#include "bridge/BridgeConstants.hh"
#include "bridge/HandBase.hh"
#include "MockCard.hh"
#include "MockHand.hh"
#include "Utility.hh"

#include <gtest/gtest.h>

#include <array>

using Bridge::Hand;
using Bridge::HandBase;
using Bridge::N_CARDS_PER_PLAYER;

using testing::ElementsAre;
using testing::Return;
using testing::Values;

namespace {

class MockHandBase : public Bridge::HandBase {
public:
    using HandBase::HandBase;
    using HandBase::notifyAll;
    MOCK_METHOD1(handleRequestReveal, void(const IndexVector&));
};

}

class HandBaseTest : public testing::TestWithParam<std::size_t> {
protected:
    std::array<testing::NiceMock<Bridge::MockCard>, N_CARDS_PER_PLAYER> cards;
    MockHandBase hand {cards.begin(), cards.end()};
};

TEST_F(HandBaseTest, testSubscribe)
{
    const auto state = Hand::CardRevealState::REQUESTED;
    const auto observer = std::make_shared<
        Bridge::MockCardRevealStateObserver>();
    EXPECT_CALL(*observer, handleNotify(state, ElementsAre(10)));

    hand.subscribe(observer);
    hand.notifyAll(state, Hand::IndexVector {10});
}

TEST_F(HandBaseTest, testNumberOfCards)
{
    EXPECT_EQ(N_CARDS_PER_PLAYER, hand.getNumberOfCards());
}

TEST_P(HandBaseTest, testUnplayedCards)
{
    const auto n = GetParam();
    EXPECT_EQ(&cards[n], hand.getCard(n));
}

TEST_P(HandBaseTest, testPlayedCards)
{
    const auto n = GetParam();
    hand.markPlayed(n);
    EXPECT_FALSE(hand.getCard(n));
}

INSTANTIATE_TEST_CASE_P(SamplingCards, HandBaseTest, Values(1, 2, 3, 5, 8));
