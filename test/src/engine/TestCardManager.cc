#include "bridge/BridgeConstants.hh"
#include "Utility.hh"
#include "MockCardManager.hh"
#include "MockHand.hh"
#include "MockObserver.hh"
#include "TestUtility.hh"

#include <gtest/gtest.h>

#include <memory>

using Bridge::Engine::CardManager;
using Bridge::MockHand;
using Bridge::N_CARDS;
using Bridge::to;

using testing::_;
using testing::ElementsAreArray;
using testing::Return;

class CardManagerTest : public testing::Test {
protected:
    virtual void SetUp()
    {
        ON_CALL(cardManager, handleGetHand(_))
            .WillByDefault(Return(hand));
        ON_CALL(cardManager, handleIsShuffleCompleted())
            .WillByDefault(Return(true));
        ON_CALL(cardManager, handleGetNumberOfCards())
            .WillByDefault(Return(N_CARDS));
    }

    std::shared_ptr<MockHand> hand {std::make_shared<MockHand>()};
    testing::StrictMock<Bridge::Engine::MockCardManager> cardManager;
};

TEST_F(CardManagerTest, testSubsrcibe)
{
    auto observer = std::make_shared<
        Bridge::MockObserver<CardManager::ShufflingState>>();
    EXPECT_CALL(cardManager, handleSubscribe(Bridge::WeaklyPointsTo(observer)));
    cardManager.subscribe(observer);
}

TEST_F(CardManagerTest, testRequestDeal)
{
    EXPECT_CALL(cardManager, handleRequestShuffle());
    cardManager.requestShuffle();
}

TEST_F(CardManagerTest, testShuffleNotCompleted)
{
    EXPECT_CALL(cardManager, handleIsShuffleCompleted())
        .WillOnce(Return(false));
    EXPECT_FALSE(cardManager.isShuffleCompleted());
}

TEST_F(CardManagerTest, testShuffleCompleted)
{
    EXPECT_CALL(cardManager, handleIsShuffleCompleted());
    EXPECT_TRUE(cardManager.isShuffleCompleted());
}

TEST_F(CardManagerTest, testGetNumberOfCardsWhenShuffleIsNotCompleted)
{
    EXPECT_CALL(cardManager, handleIsShuffleCompleted())
        .WillOnce(Return(false));
    EXPECT_FALSE(cardManager.getNumberOfCards());
}

TEST_F(CardManagerTest, testGetNumberOfCardsWhenShuffleIsCompleted)
{
    EXPECT_CALL(cardManager, handleIsShuffleCompleted());
    EXPECT_CALL(cardManager, handleGetNumberOfCards());
    EXPECT_EQ(N_CARDS, cardManager.getNumberOfCards());
}

TEST_F(CardManagerTest, testGetHandWhenShuffleIsNotCompleted)
{
    const auto ns = to(N_CARDS);
    EXPECT_CALL(cardManager, handleIsShuffleCompleted())
        .WillOnce(Return(false));
    EXPECT_FALSE(cardManager.getHand(ns.begin(), ns.end()));
}

TEST_F(CardManagerTest, testGetHandWhenShuffleIsCompleted)
{
    const auto ns = to(N_CARDS);
    EXPECT_CALL(cardManager, handleIsShuffleCompleted());
    EXPECT_CALL(cardManager, handleGetNumberOfCards());
    EXPECT_CALL(cardManager, handleGetHand(ElementsAreArray(ns)));
    EXPECT_EQ(hand, cardManager.getHand(ns.begin(), ns.end()));
}

TEST_F(CardManagerTest, testGetHandOutOfRange)
{
    EXPECT_CALL(cardManager, handleIsShuffleCompleted());
    EXPECT_CALL(cardManager, handleGetNumberOfCards());
    const auto ns = {N_CARDS};
    EXPECT_THROW(cardManager.getHand(ns.begin(), ns.end()), std::out_of_range);
}
