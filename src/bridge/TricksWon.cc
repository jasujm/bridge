#include "bridge/TricksWon.hh"

#include "bridge/Partnership.hh"

#include <ostream>
#include <stdexcept>

namespace Bridge {

int getNumberOfTricksWon(
    const TricksWon& tricksWon, const Partnership partnership)
{
    switch (partnership.get()) {
    case PartnershipLabel::NORTH_SOUTH:
        return tricksWon.tricksWonByNorthSouth;
    case PartnershipLabel::EAST_WEST:
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

std::ostream& operator<<(std::ostream& os, const TricksWon& tricksWon)
{
    return os << Partnerships::NORTH_SOUTH_VALUE << ": " <<
        tricksWon.tricksWonByNorthSouth << ", " <<
        Partnerships::EAST_WEST_VALUE << ": " <<
        tricksWon.tricksWonByEastWest;
}

}
