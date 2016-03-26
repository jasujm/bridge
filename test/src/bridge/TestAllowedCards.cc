#include "bridge/AllowedCards.hh"
#include "MockCard.hh"
#include "MockHand.hh"
#include "MockTrick.hh"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <array>
#include <iterator>
#include <vector>

using Bridge::CardType;
using Bridge::Rank;
using Bridge::Suit;

using testing::_;
using testing::NiceMock;
using testing::Ref;
using testing::Return;
using testing::ReturnRef;

class AllowedCardsTest : public testing::Test {
protected:
    virtual void SetUp()
    {
        ON_CALL(cards[0], handleGetType())
            .WillByDefault(Return(CardType {Rank::TWO, Suit::CLUBS}));
        ON_CALL(cards[0], handleIsKnown()).WillByDefault(Return(true));
        ON_CALL(cards[1], handleGetType())
            .WillByDefault(Return(CardType {Rank::ACE, Suit::SPADES}));
        ON_CALL(cards[1], handleIsKnown()).WillByDefault(Return(true));
        ON_CALL(hand, handleGetNumberOfCards()).WillByDefault(Return(2));
        ON_CALL(hand, handleGetCard(0)).WillByDefault(ReturnRef(cards[0]));
        ON_CALL(hand, handleGetCard(1)).WillByDefault(ReturnRef(cards[1]));
        ON_CALL(hand, handleIsPlayed(_)).WillByDefault(Return(false));
        ON_CALL(trick, handleGetHand(0)).WillByDefault(ReturnRef(hand));
        ON_CALL(trick, handleIsPlayAllowed(Ref(hand), Ref(cards[0])))
            .WillByDefault(Return(true));
        ON_CALL(trick, handleIsPlayAllowed(Ref(hand), Ref(cards[1])))
            .WillByDefault(Return(false));
    }

    std::array<NiceMock<Bridge::MockCard>, 2> cards;
    NiceMock<Bridge::MockHand> hand;
    NiceMock<Bridge::MockTrick> trick;
    std::vector<CardType> allowed_cards;
};

TEST_F(AllowedCardsTest, testAllowedCards)
{
    ON_CALL(trick, handleGetNumberOfCardsPlayed()).WillByDefault(Return(0));
    getAllowedCards(trick, std::back_inserter(allowed_cards));
    ASSERT_EQ(1u, allowed_cards.size());
    ASSERT_EQ(cards[0].getType(), allowed_cards[0]);
}

TEST_F(AllowedCardsTest, testNoAllowedCardsWhenTrickIsCompleted)
{
    ON_CALL(trick, handleGetNumberOfCardsPlayed())
        .WillByDefault(Return(Bridge::Trick::N_CARDS_IN_TRICK));
    getAllowedCards(trick, std::back_inserter(allowed_cards));
    EXPECT_TRUE(allowed_cards.empty());
}
