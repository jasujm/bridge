#include "bridge/DealResult.hh"

#include "bridge/Partnership.hh"

#include <stdexcept>

namespace Bridge {

int getNumberOfTricksWon(
    const DealResult& dealResult, const Partnership partnership)
{
    switch (partnership) {
    case Partnership::NORTH_SOUTH:
        return dealResult.tricksWonByNorthSouth;
    case Partnership::EAST_WEST:
        return dealResult.tricksWonByEastWest;
    default:
        throw std::invalid_argument("Invalid partnership");
    }
}

bool operator==(const DealResult& lhs, const DealResult& rhs)
{
    return &lhs == &rhs ||
        (lhs.tricksWonByNorthSouth == rhs.tricksWonByNorthSouth &&
         lhs.tricksWonByEastWest == rhs.tricksWonByEastWest);
}

}
