#include "bridge/CardType.hh"
#include "bridge/SimpleCard.hh"

#include <gtest/gtest.h>

using Bridge::SimpleCard;

namespace {
constexpr auto CARD_TYPE = Bridge::CardType {
    Bridge::Ranks::TWO, Bridge::Suits::SPADES};
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
