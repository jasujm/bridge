#include "bridge/BridgeConstants.hh"
#include "bridge/CardType.hh"
#include "bridge/CardTypeIterator.hh"
#include "bridge/Hand.hh"
#include "engine/SimpleCardManager.hh"
#include "Utility.hh"
#include "MockObserver.hh"

#include <boost/optional/optional.hpp>
#include <gtest/gtest.h>

#include <algorithm>
#include <memory>
#include <utility>
#include <vector>

using Bridge::CardType;
using Bridge::cardTypeIterator;
using Bridge::Hand;
using Bridge::N_CARDS;
using Bridge::Suit;
using Bridge::to;
using Bridge::Engine::SimpleCardManager;

namespace {

const auto ALL_CARDS_BEGIN = cardTypeIterator(0);
const auto ALL_CARDS_END = cardTypeIterator(N_CARDS);

auto getCardTypes(const Hand& hand)
{
    std::vector<CardType> card_types;
    for (const auto& card : hand) {
        if (const auto type = card.getType()) {
            card_types.emplace_back(*type);
        }
    }
    return card_types;
}

auto getCardTypes(SimpleCardManager& cardManager)
{
    cardManager.requestShuffle();
    const auto& range = to(N_CARDS);
    const auto hand = cardManager.getHand(range.begin(), range.end());

    if (hand) {
        return getCardTypes(*hand);
    }
    return std::vector<CardType> {};
}

}

class SimpleCardManagerTest : public testing::Test {
protected:
    virtual void SetUp()
    {
        cardManager.subscribe(shuffledObserver);
    }

    using ShuffledObserver =
        testing::NiceMock<Bridge::MockObserver<Bridge::Engine::Shuffled>>;
    SimpleCardManager cardManager;
    std::shared_ptr<ShuffledObserver> shuffledObserver {
        std::make_shared<ShuffledObserver>() };
};

TEST_F(SimpleCardManagerTest, testInitialDeck)
{
    EXPECT_FALSE(cardManager.isShuffleCompleted());
}

TEST_F(SimpleCardManagerTest, testRequestShuffle)
{
    EXPECT_CALL(*shuffledObserver, handleNotify(testing::_));
    cardManager.requestShuffle();
}

TEST_F(SimpleCardManagerTest, testGetNumberOfCards)
{
    cardManager.requestShuffle();
    EXPECT_EQ(N_CARDS, cardManager.getNumberOfCards());
}

TEST_F(SimpleCardManagerTest, testAllPlayingCardsAreIncluded)
{
    const auto cards = getCardTypes(cardManager);
    EXPECT_TRUE(
        std::is_permutation(
            cards.begin(), cards.end(),
            ALL_CARDS_BEGIN, ALL_CARDS_END));
}

TEST_F(SimpleCardManagerTest, testCardsAreShuffled)
{
    // The probability of this test failing is 1/52!
    const auto cards = getCardTypes(cardManager);
    EXPECT_FALSE(
        std::equal(
            cards.begin(), cards.end(),
            ALL_CARDS_BEGIN, ALL_CARDS_END));
}
