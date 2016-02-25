#include "bridge/CardType.hh"
#include "MockCard.hh"
#include "MockHand.hh"
#include "MockTrick.hh"
#include "Enumerate.hh"
#include "Utility.hh"
#include "Zip.hh"

#include <gtest/gtest.h>

#include <algorithm>
#include <array>

using Bridge::Card;
using Bridge::to;
using Bridge::Trick;

using testing::_;
using testing::NiceMock;
using testing::Ref;
using testing::Return;
using testing::ReturnRef;
using testing::ValuesIn;

class TrickTest : public testing::TestWithParam<std::size_t> {
protected:
    virtual void SetUp()
    {
        for (const auto e : enumerate(zip(cards, hands))) {
            ON_CALL(trick, handleGetCard(e.first))
                .WillByDefault(ReturnRef(e.second.get<0>()));
            ON_CALL(trick, handleGetHand(e.first))
                .WillByDefault(ReturnRef(e.second.get<1>()));
            ON_CALL(e.second.get<0>(), handleIsKnown())
                .WillByDefault(Return(true));
        }
        ON_CALL(trick, handleIsPlayAllowed(_, _))
            .WillByDefault(Return(true));
    }

    std::array<NiceMock<Bridge::MockCard>,
               Bridge::Trick::N_CARDS_IN_TRICK> cards;
    std::array<Bridge::MockHand, Bridge::Trick::N_CARDS_IN_TRICK> hands;
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

TEST_P(TrickTest, testPlayWhenHandHasTurnAndPlayIsAllowed)
{
    const auto n = GetParam();
    EXPECT_CALL(trick, handleGetNumberOfCardsPlayed()).WillOnce(Return(n));
    EXPECT_CALL(trick, handleAddCardToTrick(Ref(cards[n])));
    EXPECT_TRUE(trick.play(hands[n], cards[n]));
}

TEST_P(TrickTest, testCanPlayWhenHandHasTurnAndPlayIsAllowed)
{
    const auto n = GetParam();
    EXPECT_CALL(trick, handleGetNumberOfCardsPlayed()).WillOnce(Return(n));
    EXPECT_TRUE(trick.canPlay(hands[n], cards[n]));
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

TEST_F(TrickTest, testPlayWhenPlayIsNotAllowed)
{
    EXPECT_CALL(trick, handleIsPlayAllowed(Ref(hands[0]), Ref(cards[0])))
        .WillOnce(Return(false));
    EXPECT_CALL(trick, handleAddCardToTrick(_)).Times(0);
    EXPECT_FALSE(trick.play(hands[0], cards[0]));
}

TEST_F(TrickTest, testCanPlayWhenPlayIsNotAllowed)
{
    EXPECT_CALL(trick, handleIsPlayAllowed(Ref(hands[0]), Ref(cards[0])))
        .WillOnce(Return(false));
    EXPECT_FALSE(trick.canPlay(hands[0], cards[0]));
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
    EXPECT_FALSE(trick.getWinner());
}

TEST_P(TrickTest, testGetWinnerWhenTrickIsCompleted)
{
    const auto& hand = hands[GetParam()];
    EXPECT_CALL(trick, handleGetNumberOfCardsPlayed())
        .WillOnce(Return(Trick::N_CARDS_IN_TRICK));
    EXPECT_CALL(trick, handleGetWinner()).WillOnce(ReturnRef(hand));
    EXPECT_EQ(&hand, trick.getWinner());
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
    const auto z = zip(hands, cards);
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

INSTANTIATE_TEST_CASE_P(
    SamplingCards, TrickTest, ValuesIn(to(Trick::N_CARDS_IN_TRICK)));
