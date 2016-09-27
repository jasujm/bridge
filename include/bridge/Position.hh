/** \file
 *
 * \brief Definition of Bridge::Position enum and related utilities
 */

#ifndef POSITION_HH_
#define POSITION_HH_

#include "bridge/BridgeConstants.hh"

#include <boost/bimap/bimap.hpp>

#include <array>
#include <iosfwd>
#include <string>

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
std::size_t positionOrder(Position position);

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
