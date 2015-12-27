/** \file
 *
 * \brief Definition of Bridge::Bid struct and related concepts
 */

#ifndef BID_HH_
#define BID_HH_

#include <boost/operators.hpp>

#include <array>
#include <iosfwd>
#include <cstddef>

#include <stdexcept>

namespace Bridge {

/** \brief Strain (denomination) of bridge bid
 */
enum class Strain {
    CLUBS,
    DIAMONDS,
    HEARTS,
    SPADES,
    NO_TRUMP
};

/** \brief Number of strains
 *
 * \sa Strain
 */
constexpr std::size_t N_STRAINS = 5;

/** \brief Array containing all strains
 *
 * \sa Strain
 */
constexpr std::array<Strain, N_STRAINS> STRAINS {
    Strain::CLUBS,
    Strain::DIAMONDS,
    Strain::HEARTS,
    Strain::SPADES,
    Strain::NO_TRUMP
};

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

/** \brief Equality operator for bids
 *
 * \sa Bid
 */
bool operator==(const Bid&, const Bid&);

/** \brief Less-than operator for bids
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

/** \brief Input a Strain from stream
 *
 * \param is the input stream
 * \param strain the strain to input
 *
 * \return parameter \p is
 */
std::istream& operator>>(std::istream& is, Strain& strain);

}

#endif // BID_HH_
