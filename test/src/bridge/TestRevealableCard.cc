#include "bridge/CardType.hh"
#include "bridge/RevealableCard.hh"

#include <boost/optional/optional.hpp>
#include <gtest/gtest.h>

namespace {
constexpr Bridge::CardType CARD_TYPE {Bridge::Rank::TWO, Bridge::Suit::SPADES};
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
