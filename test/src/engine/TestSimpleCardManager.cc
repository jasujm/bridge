#include "engine/SimpleCardManager.hh"

#include "bridge/BridgeConstants.hh"
#include "bridge/CardType.hh"
#include "bridge/CardTypeIterator.hh"
#include "bridge/Hand.hh"
#include "MockHand.hh"
#include "MockObserver.hh"
#include "Utility.hh"

#include <gtest/gtest.h>

#include <algorithm>
#include <vector>

using testing::_;
using testing::ElementsAreArray;
using testing::InSequence;
using testing::InvokeWithoutArgs;

using Bridge::N_CARDS;
using Bridge::Engine::CardManager;

class SimpleCardManagerTest : public testing::Test {
protected:
    using CardTypeVector = std::vector<Bridge::CardType>;

    void shuffleCards()
    {
        // Generate predictable yet not quite ordered sequence of cards
        std::minstd_rand0 re;
        cards = CardTypeVector(
            Bridge::cardTypeIterator(0),
            Bridge::cardTypeIterator(N_CARDS));
        std::shuffle(cards.begin(), cards.end(), re);
        cardManager.shuffle(cards.begin(), cards.end());
    }

    CardTypeVector cards;
    Bridge::Engine::SimpleCardManager cardManager;
};

TEST_F(SimpleCardManagerTest, testInitiallyShuffleIsNotCompleted)
{
    EXPECT_FALSE(cardManager.isShuffleCompleted());
}

TEST_F(SimpleCardManagerTest, testShuffleInIdleStateDoesNotCompleteShuffle)
{
    shuffleCards();
    EXPECT_FALSE(cardManager.isShuffleCompleted());
}

TEST_F(
    SimpleCardManagerTest, testShuffleInShuffleRequestedStateCompletesShuffle)
{
    cardManager.requestShuffle();
    shuffleCards();
    EXPECT_TRUE(cardManager.isShuffleCompleted());
}

TEST_F(SimpleCardManagerTest, testRequestShuffleNotifiesObserver)
{
    auto observer = std::make_shared<
        Bridge::MockObserver<CardManager::ShufflingState>>();
    EXPECT_CALL(
        *observer, handleNotify(CardManager::ShufflingState::REQUESTED));
    cardManager.subscribe(observer);
    cardManager.requestShuffle();
}

TEST_F(
    SimpleCardManagerTest, testShuffleCompletedNotifiesObservers)
{
    cardManager.requestShuffle();

    auto observer = std::make_shared<
        Bridge::MockObserver<CardManager::ShufflingState>>();
    cardManager.subscribe(observer);
    EXPECT_CALL(
        *observer,
        handleNotify(CardManager::ShufflingState::COMPLETED)).WillOnce(
        InvokeWithoutArgs(
            [this]()
            {
                // Verify that shuffle is already completed when the observer
                // gets notification
                EXPECT_TRUE(cardManager.isShuffleCompleted());
            }));
    shuffleCards();
}

TEST_F(SimpleCardManagerTest, testNumberOfCards)
{
    cardManager.requestShuffle();
    shuffleCards();
    EXPECT_EQ(N_CARDS, cardManager.getNumberOfCards());
}

TEST_F(SimpleCardManagerTest, testGetHand)
{
    cardManager.requestShuffle();
    shuffleCards();
    const auto range = Bridge::to(N_CARDS);
    const auto hand = cardManager.getHand(range.begin(), range.end());
    ASSERT_TRUE(hand);
    EXPECT_TRUE(
        std::equal(
            cards.begin(), cards.end(),
            hand->begin(), hand->end(),
            [](const auto& card_type, const auto& card)
            {
                return card_type == card.getType();
            }));
}

TEST_F(SimpleCardManagerTest, testRevealHand)
{
    using State = Bridge::Hand::CardRevealState;

    cardManager.requestShuffle();
    shuffleCards();
    const auto range = Bridge::to(N_CARDS);
    const auto hand = cardManager.getHand(range.begin(), range.end());
    ASSERT_TRUE(hand);
    const auto observer = std::make_shared<
        Bridge::MockCardRevealStateObserver>();
    {
        InSequence sequence;
        EXPECT_CALL(
            *observer, handleNotify(State::REQUESTED, ElementsAreArray(range)));
        EXPECT_CALL(
            *observer, handleNotify(State::COMPLETED, ElementsAreArray(range)));
    }
    hand->subscribe(observer);
    hand->requestReveal(range.begin(), range.end());
}

TEST_F(SimpleCardManagerTest, testRequestingShuffleWhenShuffleIsCompleted)
{
    cardManager.requestShuffle();
    shuffleCards();
    cardManager.requestShuffle();
    EXPECT_FALSE(cardManager.isShuffleCompleted());
}
