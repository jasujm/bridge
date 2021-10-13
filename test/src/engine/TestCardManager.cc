#include "bridge/BridgeConstants.hh"
#include "Utility.hh"
#include "MockCard.hh"
#include "MockCardManager.hh"
#include "MockHand.hh"
#include "MockObserver.hh"
#include "TestUtility.hh"

#include <gtest/gtest.h>

#include <memory>

using Bridge::Engine::CardManager;
using Bridge::MockCard;
using Bridge::MockHand;
using Bridge::N_CARDS;
using Bridge::to;

using testing::_;
using testing::ElementsAreArray;
using testing::Return;
using testing::ReturnRef;

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

    MockCard card;
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
    EXPECT_CALL(
        cardManager, handleGetHand(ElementsAreArray(Bridge::vectorize(ns))));
    EXPECT_EQ(hand, cardManager.getHand(ns.begin(), ns.end()));
}

TEST_F(CardManagerTest, testGetHandOutOfRange)
{
    EXPECT_CALL(cardManager, handleIsShuffleCompleted());
    EXPECT_CALL(cardManager, handleGetNumberOfCards());
    const auto ns = {N_CARDS};
    EXPECT_THROW(cardManager.getHand(ns.begin(), ns.end()), std::out_of_range);
}

TEST_F(CardManagerTest, testGetCardWhenShuffleIsNotCompleted)
{
    EXPECT_CALL(cardManager, handleIsShuffleCompleted())
        .WillOnce(Return(false));
    EXPECT_EQ(nullptr, cardManager.getCard(0));
}

TEST_F(CardManagerTest, testGetCardWhenShuffleIsCompleted)
{
    EXPECT_CALL(cardManager, handleIsShuffleCompleted());
    EXPECT_CALL(cardManager, handleGetNumberOfCards());
    EXPECT_CALL(cardManager, handleGetCard(0)).WillOnce(ReturnRef(card));
    EXPECT_EQ(&card, cardManager.getCard(0));
}

TEST_F(CardManagerTest, testGetCardOutOfRange)
{
    EXPECT_CALL(cardManager, handleIsShuffleCompleted());
    EXPECT_CALL(cardManager, handleGetNumberOfCards());
    EXPECT_THROW(cardManager.getCard(N_CARDS), std::out_of_range);
}
