#include "bridge/BridgeConstants.hh"
#include "Utility.hh"
#include "MockCardManager.hh"
#include "MockHand.hh"

#include <gtest/gtest.h>

#include <memory>
#include <vector>

using Bridge::MockHand;
using Bridge::N_CARDS;
using Bridge::to;

using testing::_;
using testing::InvokeWithoutArgs;
using testing::Return;

class CardManagerTest : public testing::Test {
protected:
    virtual void SetUp()
    {
        auto func = []() { return std::make_unique<MockHand>(); };
        ON_CALL(cardManager, handleGetHand(_))
            .WillByDefault(InvokeWithoutArgs(func));
        ON_CALL(cardManager, handleGetNumberOfCards())
            .WillByDefault(Return(N_CARDS));
    }

    Bridge::Engine::MockCardManager cardManager;
};

TEST_F(CardManagerTest, testRequestDeal)
{
    EXPECT_CALL(cardManager, handleRequestShuffle());
    cardManager.requestShuffle();
}

TEST_F(CardManagerTest, testShuffleNotCompleted)
{
    EXPECT_CALL(cardManager, handleGetNumberOfCards()).WillOnce(Return(0));
    EXPECT_FALSE(cardManager.isShuffleCompleted());
}

TEST_F(CardManagerTest, testShuffleCompleted)
{
    EXPECT_CALL(cardManager, handleGetNumberOfCards());
    EXPECT_TRUE(cardManager.isShuffleCompleted());
}

TEST_F(CardManagerTest, testGetNumberOfCards)
{
    EXPECT_CALL(cardManager, handleGetNumberOfCards());
    EXPECT_EQ(N_CARDS, cardManager.getNumberOfCards());
}

TEST_F(CardManagerTest, testGetHand)
{
    const auto& range = to(N_CARDS);
    const auto ns = std::vector<std::size_t>(range.begin(), range.end());
    EXPECT_CALL(cardManager, handleGetNumberOfCards());
    EXPECT_CALL(cardManager, handleGetHand(ns));
    cardManager.getHand(ns.begin(), ns.end());
}

TEST_F(CardManagerTest, testGetHandOutOfRange)
{
    EXPECT_CALL(cardManager, handleGetNumberOfCards());
    const auto ns {N_CARDS};
    EXPECT_THROW(cardManager.getHand(ns.begin(), ns.end()), std::out_of_range);
}
