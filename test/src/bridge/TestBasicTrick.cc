#include "bridge/BasicTrick.hh"
#include "bridge/CardType.hh"
#include "Enumerate.hh"
#include "Zip.hh"
#include "MockCard.hh"
#include "MockHand.hh"

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
        for (auto&& e : enumerate(zip(hands, cards))) {
            ON_CALL(e.second.get<0>(), handleIsOutOfSuit(_))
                .WillByDefault(Return(false));
            ON_CALL(e.second.get<1>(), handleGetType())
                .WillByDefault(
                    Return(CardType {Bridge::RANKS.at(e.first), Suit::SPADES}));
            ON_CALL(e.second.get<1>(), handleIsKnown())
                .WillByDefault(Return(true));
        }
    }

    std::array<NiceMock<Bridge::MockHand>, Trick::N_CARDS_IN_TRICK> hands;
    std::array<NiceMock<Bridge::MockCard>, Trick::N_CARDS_IN_TRICK> cards;
    Bridge::BasicTrick trick {hands.begin(), hands.end()};
};

TEST_F(BasicTrickTest, testTurns)
{
    for (auto&& e : zip(hands, cards)) {
        EXPECT_EQ(&e.get<0>(), trick.getHandInTurn());
        EXPECT_TRUE(trick.play(e.get<0>(), e.get<1>()));
    }
    EXPECT_FALSE(trick.getHandInTurn());
}

TEST_F(BasicTrickTest, testCardsPlayed)
{
    for (auto&& e : zip(hands, cards)) {
        EXPECT_FALSE(trick.getCard(e.get<0>()));
        EXPECT_TRUE(trick.play(e.get<0>(), e.get<1>()));
        EXPECT_EQ(&e.get<1>(), trick.getCard(e.get<0>()));
    }
}
