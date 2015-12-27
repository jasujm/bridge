#include "bridge/BasicTrick.hh"
#include "bridge/CardType.hh"
#include "Enumerate.hh"
#include "Zip.hh"
#include "MockCard.hh"
#include "MockHand.hh"

#include <boost/optional/optional.hpp>
#include <gtest/gtest.h>

#include <array>

using Bridge::CardType;
using Bridge::Rank;
using Bridge::Suit;
using Bridge::Trick;

using testing::_;
using testing::NiceMock;
using testing::Return;

class BasicTrickTest : public testing::Test {
protected:
    virtual void SetUp()
    {
        for (const auto e : enumerate(zip(hands, cards))) {
            ON_CALL(e.second.get<0>(), handleIsOutOfSuit(_))
                .WillByDefault(Return(false));
            ON_CALL(e.second.get<1>(), handleGetType())
                .WillByDefault(
                    Return(CardType {Bridge::RANKS.at(e.first), Suit::SPADES}));
            ON_CALL(e.second.get<1>(), handleIsKnown())
                .WillByDefault(Return(true));
        }
    }

    void playAll()
    {
        for (const auto t : zip(hands, cards)) {
            trick.play(t.get<0>(), t.get<1>());
        }
    }

    std::array<NiceMock<Bridge::MockHand>, Trick::N_CARDS_IN_TRICK> hands;
    std::array<NiceMock<Bridge::MockCard>, Trick::N_CARDS_IN_TRICK> cards;
    Bridge::BasicTrick trick {hands.begin(), hands.end(), Suit::DIAMONDS};
};

TEST_F(BasicTrickTest, testTurns)
{
    for (const auto e : zip(hands, cards)) {
        EXPECT_EQ(&e.get<0>(), trick.getHandInTurn().get_ptr());
        EXPECT_TRUE(trick.play(e.get<0>(), e.get<1>()));
    }
    EXPECT_FALSE(trick.getHandInTurn());
}

TEST_F(BasicTrickTest, testCardsPlayed)
{
    for (const auto e : zip(hands, cards)) {
        EXPECT_FALSE(trick.getCard(e.get<0>()));
        EXPECT_TRUE(trick.play(e.get<0>(), e.get<1>()));
        EXPECT_EQ(&e.get<1>(), trick.getCard(e.get<0>()).get_ptr());
    }
}

TEST_F(BasicTrickTest, testHandMustFollowSuitIfHeCan)
{
    ON_CALL(cards[1], handleGetType())
        .WillByDefault(Return(CardType {Rank::TWO, Suit::HEARTS}));

    trick.play(hands[0], cards[0]);
    EXPECT_FALSE(trick.play(hands[1], cards[1]));
}

TEST_F(BasicTrickTest, testHandUnableToFollowSuit)
{
    ON_CALL(cards[1], handleGetType())
        .WillByDefault(Return(CardType {Rank::TWO, Suit::HEARTS}));
    ON_CALL(hands[1], handleIsOutOfSuit(Suit::SPADES))
        .WillByDefault(Return(true));

    trick.play(hands[0], cards[0]);
    EXPECT_TRUE(trick.play(hands[1], cards[1]));
}

TEST_F(BasicTrickTest, testHighestCardOfOriginalSuitWinsNoTrump)
{
    ON_CALL(cards[1], handleGetType())
        .WillByDefault(Return(CardType {Rank::ACE, Suit::HEARTS}));
    ON_CALL(hands[1], handleIsOutOfSuit(Suit::SPADES))
        .WillByDefault(Return(true));

    playAll();
    EXPECT_EQ(&hands[3], trick.getWinner().get_ptr());
}

TEST_F(BasicTrickTest, testHighestTrumpWins)
{
    ON_CALL(cards[1], handleGetType())
        .WillByDefault(Return(CardType {Rank::TWO, Suit::DIAMONDS}));
    ON_CALL(hands[1], handleIsOutOfSuit(Suit::SPADES))
        .WillByDefault(Return(true));

    playAll();
    EXPECT_EQ(&hands[1], trick.getWinner().get_ptr());
}
