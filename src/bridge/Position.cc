#include "bridge/Position.hh"

#include <algorithm>

namespace Bridge {

int positionOrder(const Position position)
{
    const auto n = static_cast<int>(position.get());
    if (n < 0 || n >= Position::ssize()) {
        throw std::invalid_argument {"Invalid position"};
    }
    return n;
}

Position clockwise(const Position position, int steps)
{
    if (steps < 0) {
        const auto negative_steps = (-steps) % Position::size();
        steps = Position::size() - negative_steps;
    }

    const auto n = positionOrder(position);
    return static_cast<PositionLabel>((n + steps) % Position::size());
}

Position partnerFor(Position position)
{
    return clockwise(position, 2);
}

std::ostream& operator<<(std::ostream& os, const Position position)
{
    return os << position.value();
}

}
