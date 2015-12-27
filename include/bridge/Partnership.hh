/** \file
 *
 * \brief Definition of Bridge::Partnership enum and related utilities
 */

#ifndef PARTNERSHIP_HH_
#define PARTNERSHIP_HH_

#include <utility>

namespace Bridge {

enum class Position;

/** \brief Bridge partnership
 */
enum class Partnership {
    NORTH_SOUTH,
    EAST_WEST
};

/** \brief Determine the positions in a partnership
 *
 * \param partnership the partnership
 *
 * \return a pair of positions belonging to the given partnership
 *
 * \throw std::invalid_argument if the partnership is invalid
 */
std::pair<Position, Position> positionsFor(Partnership partnership);

/** \brief Determine to which partnership a given position belongs to
 *
 * \param position the position
 *
 * \return the partnership the given position belongs to
 *
 * \throw std::invalid_argument if the position is invalid
 */
Partnership partnershipFor(Position position);

/** \brief Determine the other partnership
 *
 * \param partnership the partnership
 *
 * \return the opponent partnership, i.e. NORTH_SOUTH for EAST_WEST and vice
 * versa
 *
 * \throw std::invalid_argument if the partnership is invalid
 */
Partnership otherPartnership(Partnership partnership);

}

#endif // PARTNERSHIP_HH_
