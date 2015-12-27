#include "bridge/Position.hh"

#include "IoUtility.hh"

#include <boost/range/irange.hpp>

#include <algorithm>
#include <map>
#include <stdexcept>

namespace Bridge {

std::vector<std::size_t> cardsFor(const Position position)
{
    const auto n = static_cast<std::size_t>(position);
    if (n >= N_POSITIONS) {
        throw std::invalid_argument("Invalid position");
    }
    const auto& range = boost::irange(
        n * N_CARDS_PER_PLAYER, (n + 1) * N_CARDS_PER_PLAYER);
    return std::vector<std::size_t>(range.begin(), range.end());
}

Position clockwise(const Position position, int steps)
{
    if (steps < 0) {
        const auto negative_steps = (-steps) % N_POSITIONS;
        steps = N_POSITIONS - negative_steps;
    }

    const auto iter = std::find(POSITIONS.begin(), POSITIONS.end(), position);
    const auto diff = iter - POSITIONS.begin();
    return POSITIONS[(diff + steps) % N_POSITIONS];
}

std::ostream& operator<<(std::ostream& os, const Position position)
{
    static const auto POSITION_NAMES = std::map<Position, const char*>{
        { Position::NORTH, "north" },
        { Position::EAST,  "east"  },
        { Position::SOUTH, "south" },
        { Position::WEST,  "west"  },
    };
    return outputEnum(os, position, POSITION_NAMES);
}

}
