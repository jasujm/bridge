#include "bridge/CardType.hh"
#include "Enumerate.hh"
#include "MockCard.hh"
#include "MockHand.hh"
#include "Utility.hh"

#include <gtest/gtest.h>

#include <array>

using Bridge::to;

using testing::_;
using testing::Ref;
using testing::Return;
using testing::ReturnRef;
using testing::ValuesIn;

namespace {
constexpr std::size_t N_CARDS {4};
}

class HandTest : public testing::TestWithParam<std::size_t> {
protected:
    virtual void SetUp()
    {
        for (const auto& e : enumerate(cards)) {
            ON_CALL(hand, handleGetCard(e.first))
                .WillByDefault(ReturnRef(e.second));
            ON_CALL(hand, handleIsPlayed(e.first))
                .WillByDefault(Return(false));
            ON_CALL(e.second, handleGetType())
                .WillByDefault(
                    Return(Bridge::CardType {Bridge::RANKS.at(e.first),
                                             Bridge::Suit::SPADES}));
            ON_CALL(e.second, handleIsKnown())
                .WillByDefault(Return(true));
        }
        ON_CALL(hand, handleGetNumberOfCards())
            .WillByDefault(Return(N_CARDS));
        ON_CALL(hand, handleIsOutOfSuit(_))
            .WillByDefault(Return(boost::indeterminate));
    }

    testing::NiceMock<Bridge::MockHand> hand;
    std::array<testing::NiceMock<Bridge::MockCard>, N_CARDS> cards;
};

TEST_P(HandTest, testMarkPlayed)
{
    const auto n = GetParam();
    EXPECT_CALL(hand, handleMarkPlayed(n));
    hand.markPlayed(n);
}

TEST_F(HandTest, testMarkPlayedOutOfRange)
{
    EXPECT_THROW(hand.markPlayed(N_CARDS), std::out_of_range);
}

TEST_P(HandTest, testGetCard)
{
    const auto n = GetParam();
    EXPECT_EQ(&cards[n], hand.getCard(n));
}

TEST_P(HandTest, testGetPlayedCard)
{
    const auto n = GetParam();
    EXPECT_CALL(hand, handleIsPlayed(n)).WillOnce(Return(true));
    EXPECT_CALL(hand, handleGetCard(n)).Times(0);
    EXPECT_FALSE(hand.getCard(n));
}

TEST_F(HandTest, testGetCardOutOfRange)
{
    EXPECT_THROW(hand.getCard(N_CARDS), std::out_of_range);
}

TEST_P(HandTest, testIsPlayedWhenCardsAreNotPlayed)
{
    const auto n = GetParam();
    EXPECT_FALSE(hand.isPlayed(n));
}

TEST_P(HandTest, testIsPlayedWhenCardsArePlayed)
{
    const auto n = GetParam();
    EXPECT_CALL(hand, handleIsPlayed(n)).WillOnce(Return(true));
    EXPECT_TRUE(hand.isPlayed(n));
}

TEST_F(HandTest, testIsPlayedOutOfRange)
{
    EXPECT_THROW(hand.isPlayed(N_CARDS), std::out_of_range);
}

TEST_F(HandTest, testIsKnownToBeOutOfSuit)
{
    EXPECT_CALL(hand, handleIsOutOfSuit(Bridge::Suit::SPADES))
        .WillOnce(Return(true));
    EXPECT_TRUE(hand.isOutOfSuit(Bridge::Suit::SPADES));
}

TEST_F(HandTest, testIsOutOfSuitWhenNoSuitDealt)
{
    EXPECT_TRUE(hand.isOutOfSuit(Bridge::Suit::HEARTS));
}

TEST_F(HandTest, testIsOutOfSuitWhenSuitIsPlayed)
{
    for (const auto i : to(N_CARDS)) {
        EXPECT_CALL(hand, handleIsPlayed(i)).WillOnce(Return(true));
    }
    EXPECT_TRUE(hand.isOutOfSuit(Bridge::Suit::SPADES));
}

TEST_F(HandTest, testIsKnownToNotBeOutOfSuit)
{
    EXPECT_CALL(hand, handleIsOutOfSuit(Bridge::Suit::HEARTS))
        .WillOnce(Return(false));
    EXPECT_FALSE(hand.isOutOfSuit(Bridge::Suit::HEARTS));
}

TEST_F(HandTest, testIsNotOutOfSuit)
{
    EXPECT_FALSE(hand.isOutOfSuit(Bridge::Suit::SPADES));
}

TEST_F(HandTest, testIsOutOfSuitIsIndeterminateWhenCardsAreNotKnown)
{
    EXPECT_CALL(cards[3], handleIsKnown()).WillOnce(Return(false));
    EXPECT_TRUE(boost::indeterminate(hand.isOutOfSuit(Bridge::Suit::HEARTS)));
}

TEST_F(HandTest, testIsNotOutOfSuitIsWhenCardsAreNotKnown)
{
    EXPECT_CALL(cards[0], handleIsKnown()).WillOnce(Return(false));
    EXPECT_FALSE(hand.isOutOfSuit(Bridge::Suit::SPADES));
}

TEST_F(HandTest, testFindCardSuccessfully)
{
    EXPECT_EQ(std::size_t {3}, findFromHand(hand, *cards[3].getType()));
}

TEST_F(HandTest, testFindCardIfTypeIsAlreadyPlayed)
{
    ON_CALL(hand, handleIsPlayed(3)).WillByDefault(Return(true));
    EXPECT_FALSE(findFromHand(hand, *cards[3].getType()));
}

INSTANTIATE_TEST_CASE_P(SamplingCards, HandTest, ValuesIn(to(N_CARDS)));
