#include "bridge/Position.hh"

#include "IoUtility.hh"

#include <algorithm>

namespace Bridge {

namespace {

const auto POSITION_STRING_PAIRS =
    std::initializer_list<PositionToStringMap::value_type> {
    { Position::NORTH, "north" },
    { Position::EAST,  "east"  },
    { Position::SOUTH, "south" },
    { Position::WEST,  "west"  }};

}

const PositionToStringMap POSITION_TO_STRING_MAP(
    POSITION_STRING_PAIRS.begin(), POSITION_STRING_PAIRS.end());

std::size_t positionOrder(const Position position)
{
    if (
        std::find(
            POSITIONS.begin(), POSITIONS.end(), position) == POSITIONS.end()) {
        throw std::invalid_argument {"Invalid position"};
    }
    const auto n = static_cast<std::size_t>(position);
    assert(n < N_POSITIONS);
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
