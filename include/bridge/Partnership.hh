/** \file
 *
 * \brief Definition of Bridge::Partnership enum and related utilities
 */

#ifndef PARTNERSHIP_HH_
#define PARTNERSHIP_HH_

#include "enhanced_enum/enhanced_enum.hh"

#include <iosfwd>
#include <utility>
#include <string_view>

namespace Bridge {

struct Position;

/// \cond internal

/*[[[cog
import cog
import enumecg
import enum
class Partnership(enum.Enum):
    NORTH_SOUTH = "northSouth"
    EAST_WEST = "eastWest"
cog.out(enumecg.generate(Partnership, primary_type="enhanced"))
]]]*/
enum class PartnershipLabel {
    NORTH_SOUTH,
    EAST_WEST,
};

struct Partnership : ::enhanced_enum::enum_base<Partnership, PartnershipLabel, std::string_view> {
    using ::enhanced_enum::enum_base<Partnership, PartnershipLabel, std::string_view>::enum_base;
    static constexpr std::array values {
        value_type { "northSouth" },
        value_type { "eastWest" },
    };
};

constexpr Partnership enhance(PartnershipLabel e) noexcept
{
    return e;
}

namespace Partnerships {
inline constexpr const Partnership::value_type& NORTH_SOUTH_VALUE { std::get<0>(Partnership::values) };
inline constexpr const Partnership::value_type& EAST_WEST_VALUE { std::get<1>(Partnership::values) };
inline constexpr Partnership NORTH_SOUTH { PartnershipLabel::NORTH_SOUTH };
inline constexpr Partnership EAST_WEST { PartnershipLabel::EAST_WEST };
}
//[[[end]]]

/// \endcond

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
