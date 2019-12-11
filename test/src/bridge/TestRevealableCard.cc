#include "bridge/CardType.hh"
#include "bridge/RevealableCard.hh"

#include <gtest/gtest.h>

namespace {
constexpr auto CARD_TYPE = Bridge::CardType {
    Bridge::Ranks::TWO, Bridge::Suits::SPADES};
}

class RevealableCardTest : public testing::Test {
protected:
    Bridge::RevealableCard card;
};

TEST_F(RevealableCardTest, testInitiallyCardTypeIsUnknown)
{
    EXPECT_FALSE(card.isKnown());
}

TEST_F(RevealableCardTest, testAfterRevealingCardTypeIsKnown)
{
    card.reveal(CARD_TYPE);
    EXPECT_EQ(CARD_TYPE, card.getType());
}
