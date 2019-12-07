#include "bridge/BridgeConstants.hh"
#include "bridge/CardsForPosition.hh"
#include "bridge/Position.hh"
#include "Utility.hh"

#include <algorithm>
#include <stdexcept>

#include <boost/iterator/counting_iterator.hpp>
#include <gtest/gtest.h>

using Bridge::Position;
using Bridge::PositionLabel;
namespace Positions = Bridge::Positions;

class PositionTest : public testing::TestWithParam<Position> {};

TEST_P(PositionTest, testCardsForPositionHasCorrectSize)
{
    const auto position = GetParam();
    EXPECT_EQ(Bridge::N_CARDS_PER_PLAYER, Bridge::ssize(cardsFor(position)));
}

TEST_F(PositionTest, testInvalidPosition)
{
    EXPECT_THROW(cardsFor(static_cast<PositionLabel>(-1)), std::invalid_argument);
}

TEST_F(PositionTest, testCardsForEachPositionAreUnique)
{
    auto all_cards = std::vector<int> {};
    for (const auto position : Position::all()) {
        const auto cards = cardsFor(position);
        std::copy(cards.begin(), cards.end(), std::back_inserter(all_cards));
    }
    EXPECT_TRUE(
        std::is_permutation(
            all_cards.begin(), all_cards.end(),
            boost::make_counting_iterator(0),
            boost::make_counting_iterator(Bridge::N_CARDS)));
}

INSTANTIATE_TEST_SUITE_P(
    Positions, PositionTest,
    testing::ValuesIn(Position::begin(), Position::end()));
