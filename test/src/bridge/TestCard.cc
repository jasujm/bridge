#include "bridge/CardType.hh"
#include "MockCard.hh"

#include <gtest/gtest.h>

using testing::Return;

namespace {
constexpr Bridge::CardType CARD_TYPE {Bridge::Rank::TWO, Bridge::Suit::SPADES};
}

class CardTest : public testing::Test {
protected:
    virtual void SetUp()
    {
        ON_CALL(card, handleIsKnown()).WillByDefault(Return(true));
    }

    testing::NiceMock<Bridge::MockCard> card;
};

TEST_F(CardTest, testGetKnownCardType)
{
    EXPECT_CALL(card, handleGetType()).WillOnce(Return(CARD_TYPE));
    EXPECT_EQ(CARD_TYPE, *card.getType());
}

TEST_F(CardTest, testGetUnknownCardType)
{
    EXPECT_CALL(card, handleIsKnown()).WillOnce(Return(false));
    EXPECT_CALL(card, handleGetType()).Times(0);
    EXPECT_FALSE(card.getType());
}
