/** \file
 *
 * \brief Definition of Bridge::Position enum and related utilities
 */

#ifndef POSITION_HH_
#define POSITION_HH_

#include "bridge/BridgeConstants.hh"

#include <boost/bimap/bimap.hpp>

#include <array>
#include <cstddef>
#include <iosfwd>
#include <string>
#include <vector>

namespace Bridge {

/** \brief Bridge player position
 */
enum class Position {
    NORTH,
    EAST,
    SOUTH,
    WEST
};

/** \brief Number of possible positions
 *
 * \sa Position
 */
constexpr std::size_t N_POSITIONS = N_PLAYERS;

/** \brief Array containing all positions
 *
 * \sa Position
 */
constexpr std::array<Position, N_POSITIONS> POSITIONS {
    Position::NORTH,
    Position::EAST,
    Position::SOUTH,
    Position::WEST,
};

/** \brief Type of \ref POSITION_TO_STRING_MAP
 */
using PositionToStringMap = boost::bimaps::bimap<Position, std::string>;

/** \brief Two-way map between Position enumerations and their string
 * representation
 */
extern const PositionToStringMap POSITION_TO_STRING_MAP;

/** \brief Determine the indices of cards dealt to given position
 *
 * \param position the position
 *
 * \return A set of 13 indices between 0–51, each disjoint from the set for
 * any other position.
 *
 * \throw std::invalid_argument if position is invalid
 */
std::vector<std::size_t> cardsFor(Position position);

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
 */
Position clockwise(Position position, int steps = 1);

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
