#include "bridge/CardType.hh"
#include "MockCard.hh"
#include "MockHand.hh"
#include "MockTrick.hh"
#include "Enumerate.hh"
#include "Utility.hh"

#include <boost/range/combine.hpp>
#include <gtest/gtest.h>

#include <algorithm>
#include <array>

using Bridge::Card;
using Bridge::CardType;
using Bridge::to;
using Bridge::Trick;

using testing::_;
using testing::NiceMock;
using testing::Ref;
using testing::Return;
using testing::ReturnRef;
using testing::ValuesIn;

namespace Ranks = Bridge::Ranks;
namespace Suits = Bridge::Suits;

class TrickTest : public testing::TestWithParam<int> {
protected:
    virtual void SetUp()
    {
        for (auto&& e : enumerate(boost::combine(cards, hands))) {
            ON_CALL(trick, handleGetCard(e.first))
                .WillByDefault(ReturnRef(e.second.get<0>()));
            ON_CALL(trick, handleGetHand(e.first))
                .WillByDefault(ReturnRef(e.second.get<1>()));
            ON_CALL(e.second.get<0>(), handleIsKnown())
                .WillByDefault(Return(true));
        }
    }

    void setCardTypes(
        const CardType& card1, const CardType& card2,
        const CardType& card3, const CardType& card4)
    {
        ON_CALL(cards[0], handleGetType()).WillByDefault(Return(card1));
        ON_CALL(cards[1], handleGetType()).WillByDefault(Return(card2));
        ON_CALL(cards[2], handleGetType()).WillByDefault(Return(card3));
        ON_CALL(cards[3], handleGetType()).WillByDefault(Return(card4));
    }

    std::array<NiceMock<Bridge::MockCard>, Trick::N_CARDS_IN_TRICK> cards;
    std::array<
        NiceMock<Bridge::MockHand>, Bridge::Trick::N_CARDS_IN_TRICK> hands;
    NiceMock<Bridge::MockTrick> trick;
};

TEST_P(TrickTest, testTrickCompletionWhenIncomplete)
{
    const auto n = GetParam();
    EXPECT_CALL(trick, handleGetNumberOfCardsPlayed()).WillOnce(Return(n));
    EXPECT_FALSE(trick.isCompleted());
}

TEST_F(TrickTest, testTrickIncompletionWhenComplete)
{
    EXPECT_CALL(trick, handleGetNumberOfCardsPlayed())
        .WillOnce(Return(Bridge::Trick::N_CARDS_IN_TRICK));
    EXPECT_TRUE(trick.isCompleted());
}

TEST_F(TrickTest, testPlayWhenCardIsNotKnown)
{
    EXPECT_CALL(cards[0], handleIsKnown()).WillOnce(Return(false));
    EXPECT_CALL(trick, handleAddCardToTrick(_)).Times(0);
    EXPECT_FALSE(trick.play(hands[0], cards[0]));
}

TEST_F(TrickTest, testCanPlayWhenCardIsNotKnown)
{
    EXPECT_CALL(cards[0], handleIsKnown()).WillOnce(Return(false));
    EXPECT_FALSE(trick.canPlay(hands[0], cards[0]));
}

TEST_F(TrickTest, testPlayWhenHandHasNotTurn)
{
    EXPECT_CALL(trick, handleGetNumberOfCardsPlayed()).WillOnce(Return(1));
    EXPECT_CALL(trick, handleAddCardToTrick(_)).Times(0);
    EXPECT_FALSE(trick.play(hands[0], cards[0]));
}

TEST_F(TrickTest, testCanPlayWhenHandHasNotTurn)
{
    EXPECT_CALL(trick, handleGetNumberOfCardsPlayed()).WillOnce(Return(1));
    EXPECT_FALSE(trick.canPlay(hands[0], cards[0]));
}

TEST_F(TrickTest, testPlayWhenHandHasTurnAndTrickIsEmpty)
{
    EXPECT_CALL(trick, handleGetNumberOfCardsPlayed()).WillOnce(Return(0));
    EXPECT_CALL(trick, handleAddCardToTrick(Ref(cards[0])));
    EXPECT_TRUE(trick.play(hands[0], cards[0]));
}

TEST_P(TrickTest, testCanPlayWhenHandHasTurnAndTrickIsEmpty)
{
    EXPECT_CALL(trick, handleGetNumberOfCardsPlayed()).WillOnce(Return(0));
    EXPECT_TRUE(trick.canPlay(hands[0], cards[0]));
}

TEST_F(TrickTest, testPlayWhenHandHasTurnAndFollowsSuit)
{
    EXPECT_CALL(trick, handleGetNumberOfCardsPlayed()).WillOnce(Return(1));
    EXPECT_CALL(trick, handleAddCardToTrick(Ref(cards[1])));

    setCardTypes(
        {Ranks::TWO,   Suits::SPADES},
        {Ranks::THREE, Suits::SPADES},
        {Ranks::ACE,   Suits::CLUBS},
        {Ranks::FOUR,  Suits::CLUBS});
    EXPECT_TRUE(trick.play(hands[1], cards[1]));
}

TEST_F(TrickTest, testCanPlayWhenHandHasTurnAndFollowsSuit)
{
    EXPECT_CALL(trick, handleGetNumberOfCardsPlayed()).WillOnce(Return(1));

    setCardTypes(
        {Ranks::TWO,   Suits::SPADES},
        {Ranks::THREE, Suits::SPADES},
        {Ranks::ACE,   Suits::CLUBS},
        {Ranks::FOUR,  Suits::CLUBS});
    EXPECT_TRUE(trick.canPlay(hands[1], cards[1]));
}

TEST_F(TrickTest, testPlayWhenHandHasTurnAndIsOutOfSuit)
{
    EXPECT_CALL(trick, handleGetNumberOfCardsPlayed()).WillOnce(Return(1));
    EXPECT_CALL(trick, handleAddCardToTrick(Ref(cards[1])));
    EXPECT_CALL(hands[1], handleIsOutOfSuit(Suits::SPADES))
        .WillOnce(Return(true));

    setCardTypes(
        {Ranks::TWO,   Suits::SPADES},
        {Ranks::THREE, Suits::CLUBS},
        {Ranks::ACE,   Suits::CLUBS},
        {Ranks::FOUR,  Suits::CLUBS});
    EXPECT_TRUE(trick.play(hands[1], cards[1]));
}

TEST_F(TrickTest, testCanPlayWhenHandHasTurnAndIsOutOfSuit)
{
    EXPECT_CALL(trick, handleGetNumberOfCardsPlayed()).WillOnce(Return(1));
    EXPECT_CALL(hands[1], handleIsOutOfSuit(Suits::SPADES))
        .WillOnce(Return(true));

    setCardTypes(
        {Ranks::TWO,   Suits::SPADES},
        {Ranks::THREE, Suits::CLUBS},
        {Ranks::ACE,   Suits::CLUBS},
        {Ranks::FOUR,  Suits::CLUBS});
    EXPECT_TRUE(trick.canPlay(hands[1], cards[1]));
}

TEST_F(TrickTest, testPlayWhenHandHasTurnAndOutOfSuitIsIndeterminate)
{
    EXPECT_CALL(trick, handleGetNumberOfCardsPlayed()).WillOnce(Return(1));
    EXPECT_CALL(trick, handleAddCardToTrick(Ref(cards[1])));
    EXPECT_CALL(hands[1], handleIsOutOfSuit(Suits::SPADES))
        .WillOnce(Return(boost::indeterminate));

    setCardTypes(
        {Ranks::TWO,   Suits::SPADES},
        {Ranks::THREE, Suits::CLUBS},
        {Ranks::ACE,   Suits::CLUBS},
        {Ranks::FOUR,  Suits::CLUBS});
    EXPECT_TRUE(trick.play(hands[1], cards[1]));
}

TEST_F(TrickTest, testCanPlayWhenHandHasTurnAndOutOfSuitIsIndeterminate)
{
    EXPECT_CALL(trick, handleGetNumberOfCardsPlayed()).WillOnce(Return(1));
    EXPECT_CALL(hands[1], handleIsOutOfSuit(Suits::SPADES))
        .WillOnce(Return(boost::indeterminate));

    setCardTypes(
        {Ranks::TWO,   Suits::SPADES},
        {Ranks::THREE, Suits::CLUBS},
        {Ranks::ACE,   Suits::CLUBS},
        {Ranks::FOUR,  Suits::CLUBS});
    EXPECT_TRUE(trick.canPlay(hands[1], cards[1]));
}

TEST_F(TrickTest, testPlayWhenHandHasTurnAndDoesNotFollowSuit)
{
    EXPECT_CALL(trick, handleGetNumberOfCardsPlayed()).WillOnce(Return(1));
    EXPECT_CALL(trick, handleAddCardToTrick(_)).Times(0);
    EXPECT_CALL(hands[1], handleIsOutOfSuit(Suits::SPADES))
        .WillOnce(Return(false));

    setCardTypes(
        {Ranks::TWO,   Suits::SPADES},
        {Ranks::THREE, Suits::CLUBS},
        {Ranks::ACE,   Suits::CLUBS},
        {Ranks::FOUR,  Suits::CLUBS});
    EXPECT_FALSE(trick.play(hands[1], cards[1]));
}

TEST_F(TrickTest, testCanPlayWhenHandHasTurnAndDoesNotFollowSuit)
{
    EXPECT_CALL(trick, handleGetNumberOfCardsPlayed()).WillOnce(Return(1));
    EXPECT_CALL(hands[1], handleIsOutOfSuit(Suits::SPADES))
        .WillOnce(Return(false));

    setCardTypes(
        {Ranks::TWO,   Suits::SPADES},
        {Ranks::THREE, Suits::CLUBS},
        {Ranks::ACE,   Suits::CLUBS},
        {Ranks::FOUR,  Suits::CLUBS});
    EXPECT_FALSE(trick.canPlay(hands[1], cards[1]));
}

TEST_F(TrickTest, testGetLeader)
{
    EXPECT_EQ(&hands[0], &trick.getLeader());
}

TEST_F(TrickTest, testGetHandWhenTrickIsNotCompleted)
{
    EXPECT_EQ(&hands[0], trick.getHandInTurn());
}

TEST_F(TrickTest, testGetHandWhenTrickIsCompleted)
{
    EXPECT_CALL(trick, handleGetNumberOfCardsPlayed())
        .WillOnce(Return(Trick::N_CARDS_IN_TRICK));
    EXPECT_FALSE(trick.getHandInTurn());
}

TEST_F(TrickTest, testGetWinnerWhenTrickIsNotCompleted)
{
    EXPECT_FALSE(getWinner(trick));
}

TEST_F(TrickTest, testHighestCardOfOriginalSuitWinsNoTrump)
{
    ON_CALL(trick, handleGetNumberOfCardsPlayed())
        .WillByDefault(Return(Bridge::Trick::N_CARDS_IN_TRICK));
    setCardTypes(
        {Ranks::TWO,   Suits::SPADES},
        {Ranks::THREE, Suits::SPADES},
        {Ranks::ACE,   Suits::SPADES},
        {Ranks::FOUR,  Suits::SPADES});
    EXPECT_EQ(&hands[2], getWinner(trick));
}

TEST_F(TrickTest, testOnlyCardOfOriginalSuitWinsNoTrump)
{
    ON_CALL(trick, handleGetNumberOfCardsPlayed())
        .WillByDefault(Return(Bridge::Trick::N_CARDS_IN_TRICK));
    setCardTypes(
        {Ranks::TWO,   Suits::SPADES},
        {Ranks::THREE, Suits::CLUBS},
        {Ranks::ACE,   Suits::CLUBS},
        {Ranks::FOUR,  Suits::CLUBS});
    EXPECT_EQ(&hands[0], getWinner(trick));
}

TEST_F(TrickTest, testHighestTrumpWinsIfTrumpIsLead)
{
    ON_CALL(trick, handleGetNumberOfCardsPlayed())
        .WillByDefault(Return(Bridge::Trick::N_CARDS_IN_TRICK));
    setCardTypes(
        {Ranks::TWO,   Suits::SPADES},
        {Ranks::THREE, Suits::SPADES},
        {Ranks::ACE,   Suits::SPADES},
        {Ranks::FOUR,  Suits::SPADES});
    EXPECT_EQ(&hands[2], getWinner(trick, Suits::SPADES));
}

TEST_F(TrickTest, testHighestTrumpWinsIfTrumpIsNotLead)
{
    ON_CALL(trick, handleGetNumberOfCardsPlayed())
        .WillByDefault(Return(Bridge::Trick::N_CARDS_IN_TRICK));
    setCardTypes(
        {Ranks::ACE,   Suits::SPADES},
        {Ranks::TWO,   Suits::CLUBS},
        {Ranks::THREE, Suits::CLUBS},
        {Ranks::FOUR,  Suits::CLUBS});
    EXPECT_EQ(&hands[3], getWinner(trick, Suits::CLUBS));
}

TEST_P(TrickTest, testGetCardWhenTrickIsEmpty)
{
    const auto& hand = hands[GetParam()];
    EXPECT_FALSE(trick.getCard(hand));
}

TEST_P(TrickTest, testGetCardWhenTrickIsCompleted)
{
    const auto n = GetParam();
    EXPECT_CALL(trick, handleGetNumberOfCardsPlayed())
        .WillOnce(Return(Trick::N_CARDS_IN_TRICK));
    EXPECT_EQ(&cards[n], trick.getCard(hands[n]));
}

TEST_F(TrickTest, testCardIterators)
{
    auto&& z = boost::combine(hands, cards);
    ON_CALL(trick, handleGetNumberOfCardsPlayed())
        .WillByDefault(Return(Trick::N_CARDS_IN_TRICK));
    EXPECT_TRUE(
        std::equal(
            z.begin(), z.end(), trick.begin(), trick.end(),
            [](const auto& t, const auto& p)
            {
                return &t.template get<0>() == &p.first &&
                    &t.template get<1>() == &p.second;
            }));
}

INSTANTIATE_TEST_SUITE_P(
    SamplingCards, TrickTest, ValuesIn(to(Trick::N_CARDS_IN_TRICK)));
