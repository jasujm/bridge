/** \file
 *
 * \brief Definition of Bridge::Position enum and related utilities
 */

#ifndef POSITION_HH_
#define POSITION_HH_

#include "enhanced_enum/enhanced_enum.hh"

#include <iosfwd>
#include <string_view>

namespace Bridge {

/// \cond internal

/*[[[cog
import cog
import enumecg
import enum
class Position(enum.Enum):
    NORTH = "north"
    EAST = "east"
    SOUTH = "south"
    WEST = "west"
cog.out(enumecg.generate(Position, primary_type="enhanced"))
]]]*/
enum class PositionLabel {
    NORTH,
    EAST,
    SOUTH,
    WEST,
};

struct Position : ::enhanced_enum::enum_base<Position, PositionLabel, std::string_view> {
    using ::enhanced_enum::enum_base<Position, PositionLabel, std::string_view>::enum_base;
    static constexpr std::array values {
        value_type { "north" },
        value_type { "east" },
        value_type { "south" },
        value_type { "west" },
    };
};

constexpr Position enhance(PositionLabel e) noexcept
{
    return e;
}

namespace Positions {
inline constexpr const Position::value_type& NORTH_VALUE { std::get<0>(Position::values) };
inline constexpr const Position::value_type& EAST_VALUE { std::get<1>(Position::values) };
inline constexpr const Position::value_type& SOUTH_VALUE { std::get<2>(Position::values) };
inline constexpr const Position::value_type& WEST_VALUE { std::get<3>(Position::values) };
inline constexpr Position NORTH { PositionLabel::NORTH };
inline constexpr Position EAST { PositionLabel::EAST };
inline constexpr Position SOUTH { PositionLabel::SOUTH };
inline constexpr Position WEST { PositionLabel::WEST };
}
//[[[end]]]

/// \endcond

/** \brief Return order of the position
 *
 * In bridge positions have defined playing order north, east, south,
 * west. This function can be used to cast the position to its order in type
 * safe manner.
 *
 * \return order of \p position (between 0—3)
 *
 * \throw std::invalid_argument if \p position is not valid
 */
int positionOrder(Position position);

/** \brief Determine position clockwise from the given position
 *
 * Examples:
 *
 * \code{.cc}
 * clockwise(Position::NORTH) == Position::EAST
 * clockwise(Position::EAST, 2) == Position::WEST
 * clockwise(Position::WEST, -1) == Position::SOUTH
 * \endcode
 *
 * \param position the position from which counting starts
 * \param steps the number of steps skipped
 *
 * \return the rotated position
 *
 * \throw std::invalid_argument if \p position is invalid
 */
Position clockwise(Position position, int steps = 1);

/** \brief Determine partner of given position
 *
 * \param position the position
 *
 * \return the position sitting opposite of \p position
 *
 * \throw std::invalid_argument if \p position is invalid
 */
Position partnerFor(Position position);

/** \brief Output a Position to stream
 *
 * \param os the output stream
 * \param position the position to output
 *
 * \return parameter \p os
 */
std::ostream& operator<<(std::ostream& os, Position position);

}

#endif // POSITION_HH_
