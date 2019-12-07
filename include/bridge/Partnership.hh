/** \file
 *
 * \brief Definition of Bridge::Partnership enum and related utilities
 */

#ifndef PARTNERSHIP_HH_
#define PARTNERSHIP_HH_

#include <boost/bimap/bimap.hpp>

#include <array>
#include <iosfwd>
#include <utility>
#include <string>

namespace Bridge {

struct Position;

/** \brief Bridge partnership
 */
enum class Partnership {
    NORTH_SOUTH,
    EAST_WEST
};

/** \brief Number of partnerships
 *
 * \sa Partnership
 */
constexpr auto N_PARTNERSHIPS = 2;

/** \brief Array containing all partnerships
 *
 * \sa Partnership
 */
constexpr std::array<Partnership, N_PARTNERSHIPS> PARTNERSHIPS {
    Partnership::NORTH_SOUTH,
    Partnership::EAST_WEST,
};

/** \brief Type of \ref PARTNERSHIP_TO_STRING_MAP
 */
using PartnershipToStringMap = boost::bimaps::bimap<Partnership, std::string>;

/** \brief Two-way map between Partnership enumerations and their string
 * representation
 */
extern const PartnershipToStringMap PARTNERSHIP_TO_STRING_MAP;

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

/** \brief Output a Partnership to stream
 *
 * \param os the output stream
 * \param partnership the partnership to output
 *
 * \return parameter \p os
 */
std::ostream& operator<<(std::ostream& os, Partnership partnership);

}

#endif // PARTNERSHIP_HH_
