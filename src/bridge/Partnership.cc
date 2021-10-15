#include "bridge/Partnership.hh"
#include "bridge/Position.hh"

#include <stdexcept>

namespace Bridge {

Partnership partnershipFor(const Position position)
{
    switch (position.get()) {
    case PositionLabel::NORTH:
    case PositionLabel::SOUTH:
        return Partnerships::NORTH_SOUTH;
    case PositionLabel::EAST:
    case PositionLabel::WEST:
        return Partnerships::EAST_WEST;
    default:
        throw std::invalid_argument("Invalid position");
    }
}

Partnership otherPartnership(const Partnership partnership)
{
    switch(partnership.get()) {
    case PartnershipLabel::NORTH_SOUTH:
        return Partnerships::EAST_WEST;
    case PartnershipLabel::EAST_WEST:
        return Partnerships::NORTH_SOUTH;
    default:
        throw std::invalid_argument("Invalid partnership");
    }
}

std::ostream& operator<<(std::ostream& os, Partnership partnership)
{
    return os << partnership.value();
}

}
