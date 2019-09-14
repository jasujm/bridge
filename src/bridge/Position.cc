#include "bridge/Position.hh"

#include "IoUtility.hh"

#include <algorithm>

namespace Bridge {

namespace {

using namespace std::string_literals;
using PositionStringRelation = PositionToStringMap::value_type;

const auto POSITION_STRING_PAIRS = {
    PositionStringRelation { Position::NORTH, "north"s },
    PositionStringRelation { Position::EAST,  "east"s  },
    PositionStringRelation { Position::SOUTH, "south"s },
    PositionStringRelation { Position::WEST,  "west"s  },
};

}

const PositionToStringMap POSITION_TO_STRING_MAP(
    POSITION_STRING_PAIRS.begin(), POSITION_STRING_PAIRS.end());

int positionOrder(const Position position)
{
    if (
        std::find(
            POSITIONS.begin(), POSITIONS.end(), position) == POSITIONS.end()) {
        throw std::invalid_argument {"Invalid position"};
    }
    const auto n = static_cast<int>(position);
    return n;
}

Position clockwise(const Position position, int steps)
{
    if (steps < 0) {
        const auto negative_steps = (-steps) % N_POSITIONS;
        steps = N_POSITIONS - negative_steps;
    }

    const auto n = static_cast<int>(positionOrder(position));
    return POSITIONS[(n + steps) % N_POSITIONS];
}

Position partnerFor(Position position)
{
    return clockwise(position, 2);
}

std::ostream& operator<<(std::ostream& os, const Position position)
{
    return outputEnum(os, position, POSITION_TO_STRING_MAP.left);
}

}
