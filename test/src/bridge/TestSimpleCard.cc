#include "bridge/CardType.hh"
#include "bridge/SimpleCard.hh"

#include <gtest/gtest.h>

using Bridge::SimpleCard;

namespace {
constexpr Bridge::CardType CARD_TYPE {Bridge::Rank::TWO, Bridge::Suit::SPADES};
}

class SimpleCardTest : public testing::Test {
protected:
    SimpleCard card { CARD_TYPE };
};

TEST_F(SimpleCardTest, testGetType)
{
    EXPECT_EQ(CARD_TYPE, *card.getType());
}

TEST_F(SimpleCardTest, testIsKnown)
{
    EXPECT_TRUE(card.isKnown());
}
