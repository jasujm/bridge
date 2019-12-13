/** \file
 *
 * \brief Definition of Bridge::Bid struct and related concepts
 */

#ifndef BID_HH_
#define BID_HH_

#include "enhanced_enum/enhanced_enum.hh"

#include <boost/operators.hpp>

#include <iosfwd>
#include <optional>
#include <stdexcept>
#include <string_view>

namespace Bridge {

/// \cond internal

/*[[[cog
import cog
import enumecg
import enum
class Strain(enum.Enum):
  CLUBS = "clubs"
  DIAMONDS = "diamonds"
  HEARTS = "hearts"
  SPADES = "spades"
  NO_TRUMP = "notrump"
cog.out(enumecg.generate(Strain, primary_type="enhanced"))
]]]*/
enum class StrainLabel {
    CLUBS,
    DIAMONDS,
    HEARTS,
    SPADES,
    NO_TRUMP,
};

struct Strain : ::enhanced_enum::enum_base<Strain, StrainLabel, std::string_view> {
    using ::enhanced_enum::enum_base<Strain, StrainLabel, std::string_view>::enum_base;
    static constexpr std::array values {
        value_type { "clubs" },
        value_type { "diamonds" },
        value_type { "hearts" },
        value_type { "spades" },
        value_type { "notrump" },
    };
};

constexpr Strain enhance(StrainLabel e) noexcept
{
    return e;
}

namespace Strains {
inline constexpr const Strain::value_type& CLUBS_VALUE { std::get<0>(Strain::values) };
inline constexpr const Strain::value_type& DIAMONDS_VALUE { std::get<1>(Strain::values) };
inline constexpr const Strain::value_type& HEARTS_VALUE { std::get<2>(Strain::values) };
inline constexpr const Strain::value_type& SPADES_VALUE { std::get<3>(Strain::values) };
inline constexpr const Strain::value_type& NO_TRUMP_VALUE { std::get<4>(Strain::values) };
inline constexpr Strain CLUBS { StrainLabel::CLUBS };
inline constexpr Strain DIAMONDS { StrainLabel::DIAMONDS };
inline constexpr Strain HEARTS { StrainLabel::HEARTS };
inline constexpr Strain SPADES { StrainLabel::SPADES };
inline constexpr Strain NO_TRUMP { StrainLabel::NO_TRUMP };
}
///[[[end]]]

/// \endcond

/** \brief Bid in bridge game bidding round
 *
 * A bid consists of level and strain. The level is an integer between 1 and
 * 7.
 *
 * Bids are totally ordered, with A > B if either the level of A is higher
 * than the level of B, or if their level is same but the strain of A is
 * higher than the strain of B. That is, the order is the same as the natural
 * order of bids in a bridge auction.
 *
 * \note Boost operators library is used to ensure that the rest of the
 * comparison operators are generated with usual semantics when operator== and
 * operator< are supplied.
 */
struct Bid : private boost::totally_ordered<Bid> {

    int level;      ///< \brief Level of the bid
    Strain strain;  ///< \brief Strain of the bid

    static constexpr int MINIMUM_LEVEL = 1; ///< \brief Minimum level allowed
    static constexpr int MAXIMUM_LEVEL = 7; ///< \brief Maximum level allowed

    static const Bid LOWEST_BID; ///< \brief The lowest possible bid
    static const Bid HIGHEST_BID; ///< \brief The highest possible bid

    /** \brief Determine whether given level is valid bidding level
     *
     * \param level the level to be validated
     *
     * \return true if \ref MINIMUM_LEVEL <= level <= \ref MAXIMUM_LEVEL,
     * false otherwise
     */
    static constexpr bool levelValid(int level)
    {
        return level >= MINIMUM_LEVEL && level <= MAXIMUM_LEVEL;
    }

    Bid() = default;

    /** \brief Create new bid
     *
     * \throw std::invalid_argument if levelValid(level) == false
     *
     * \param level the level of the bid
     * \param strain the strain of the bid
     *
     * \sa levelValid()
     */
    constexpr Bid(int level, Strain strain) :
        level {levelValid(level) ?
               level : throw std::invalid_argument("Invalid level")},
        strain {strain}
    {
    }
};

/** \brief Determine the next bid in bidding order
 *
 * \param bid the bid
 *
 * \return The lowest bid greater that \p bid, or none if \p bid is already
 * the highest possible bid
 */
std::optional<Bid> nextHigherBid(const Bid& bid);

/** \brief Equality operator for bids
 *
 * \sa Bid
 */
bool operator==(const Bid&, const Bid&);

/** \brief Less than operator for bids
 *
 * \sa Bid
 */
bool operator<(const Bid&, const Bid&);

/** \brief Output a Strain to stream
 *
 * \param os the output stream
 * \param strain the strain to output
 *
 * \return parameter \p os
 */
std::ostream& operator<<(std::ostream& os, Strain strain);

/** \brief Output a Bid to stream
 *
 * \param os the output stream
 * \param bid the bid to output
 *
 * \return parameter \p os
 */
std::ostream& operator<<(std::ostream& os, const Bid& bid);

}

#endif // BID_HH_
