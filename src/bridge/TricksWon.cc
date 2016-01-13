#include "bridge/TricksWon.hh"

#include "bridge/Partnership.hh"

#include <stdexcept>

namespace Bridge {

int getNumberOfTricksWon(
    const TricksWon& tricksWon, const Partnership partnership)
{
    switch (partnership) {
    case Partnership::NORTH_SOUTH:
        return tricksWon.tricksWonByNorthSouth;
    case Partnership::EAST_WEST:
        return tricksWon.tricksWonByEastWest;
    default:
        throw std::invalid_argument("Invalid partnership");
    }
}

bool operator==(const TricksWon& lhs, const TricksWon& rhs)
{
    return &lhs == &rhs ||
        (lhs.tricksWonByNorthSouth == rhs.tricksWonByNorthSouth &&
         lhs.tricksWonByEastWest == rhs.tricksWonByEastWest);
}

}
