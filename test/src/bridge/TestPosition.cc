#include "bridge/BridgeConstants.hh"
#include "bridge/Position.hh"
#include "Enumerate.hh"

#include <algorithm>
#include <stdexcept>

#include <gtest/gtest.h>

using Bridge::POSITIONS;
using Bridge::enumerate;

TEST(PositionTest, testCardsForEachPositionAreCorrectSize)
{
    for (const auto position : POSITIONS) {
        ASSERT_EQ(cardsFor(position).size(), Bridge::N_CARDS_PER_PLAYER);
    }
}

TEST(PositionTest, testInvalidPosition)
{
    EXPECT_THROW(cardsFor(static_cast<Bridge::Position>(-1)),
                 std::invalid_argument);
}

TEST(PositionTest, testCardsForEachPositionAreUnique)
{
    auto all_cards = decltype(cardsFor(POSITIONS.front())) {};
    for (const auto position : POSITIONS) {
        const auto cards = cardsFor(position);
        std::copy(cards.begin(), cards.end(), std::back_inserter(all_cards));
    }
    std::sort(all_cards.begin(), all_cards.end());
    for (const auto e : enumerate(all_cards)) {
        ASSERT_EQ(e.second, e.first); // do not flood output with errors
    }
}
