#include "bridge/Partnership.hh"

#include "bridge/Position.hh"
#include "IoUtility.hh"

#include <stdexcept>

namespace Bridge {

namespace {

using namespace std::string_literals;
using PartnershipStringRelation = PartnershipToStringMap::value_type;

const auto PARTNERSHIP_STRING_PAIRS = {
    PartnershipStringRelation { Partnership::NORTH_SOUTH, "northSouth"s },
    PartnershipStringRelation { Partnership::EAST_WEST,   "eastWest"s   },
};

}

const PartnershipToStringMap PARTNERSHIP_TO_STRING_MAP(
    PARTNERSHIP_STRING_PAIRS.begin(), PARTNERSHIP_STRING_PAIRS.end());

std::pair<Position, Position> positionsFor(const Partnership partnership)
{
    switch (partnership) {
    case Partnership::NORTH_SOUTH:
        return std::make_pair(Position::NORTH, Position::SOUTH);
    case Partnership::EAST_WEST:
        return std::make_pair(Position::EAST, Position::WEST);
    default:
        throw std::invalid_argument("Invalid partnership");
    }
}

Partnership partnershipFor(const Position position)
{
    switch (position) {
    case Position::NORTH:
    case Position::SOUTH:
        return Partnership::NORTH_SOUTH;
    case Position::EAST:
    case Position::WEST:
        return Partnership::EAST_WEST;
    default:
        throw std::invalid_argument("Invalid position");
    }
}

Partnership otherPartnership(const Partnership partnership)
{
    switch(partnership) {
    case Partnership::NORTH_SOUTH:
        return Partnership::EAST_WEST;
    case Partnership::EAST_WEST:
        return Partnership::NORTH_SOUTH;
    default:
        throw std::invalid_argument("Invalid partnership");
    }
}

std::ostream& operator<<(std::ostream& os, Partnership partnership)
{
    return outputEnum(os, partnership, PARTNERSHIP_TO_STRING_MAP.left);
}

}
