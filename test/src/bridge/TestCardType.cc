#include "bridge/BridgeConstants.hh"
#include "bridge/CardType.hh"
#include "bridge/CardTypeIterator.hh"
#include "Utility.hh"

#include <gtest/gtest.h>

TEST(CardTypeTest, testCardTypeIndex)
{
    for (const auto n : Bridge::to(Bridge::N_CARDS)) {
        EXPECT_EQ(n, cardTypeIndex(Bridge::enumerateCardType(n)));
    }
}
