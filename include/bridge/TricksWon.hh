/** \file
 *
 * \brief Definition of Bridge::TricksWon struct
 */

#ifndef TRICKSWON_HH_
#define TRICKSWON_HH_

#include "BridgeConstants.hh"

#include <boost/operators.hpp>

#include <iosfwd>
#include <stdexcept>

namespace Bridge {

enum class Partnership;

/** \brief The tricks won in a (possibly ongoing) bridge deal
 *
 * TricksWon objects are equality comparable. They compare equal when tricks
 * won by each partnership are equal.
 *
 * \note Boost operators library is used to ensure that operator!= is
 * generated with usual semantics when operator== is supplied.
 */
struct TricksWon : private boost::equality_comparable<TricksWon> {

    /** \brief The number of tricks won by the north–south partnership
     */
    int tricksWonByNorthSouth;

    /** \brief The number of tricks won by the east–west partnership
     */
    int tricksWonByEastWest;

    TricksWon() = default;

    /** \brief Create new TricksWon
     *
     * \param tricksWonByNorthSouth the number of tricks won by the
     * north–south partnership
     * \param tricksWonByEastWest the number of tricks won by the east–west
     * partnership
     *
     * \throw std::invalid_argument if either parameter is negative or their
     * sum exceeds number of tricks in a deal
     */
    constexpr TricksWon(int tricksWonByNorthSouth, int tricksWonByEastWest) :
        tricksWonByNorthSouth {
            validateTricks(tricksWonByNorthSouth, tricksWonByEastWest)},
        tricksWonByEastWest {
            validateTricks(tricksWonByEastWest, tricksWonByNorthSouth)}
    {
    }

private:

    static constexpr int validateTricks(int tricksWon, int otherTricksWon)
    {
        return (tricksWon >= 0 &&
                tricksWon + otherTricksWon <= int {N_CARDS_PER_PLAYER}) ?
            tricksWon : throw std::invalid_argument("Invalid number of tricks");
    }
};

/** \brief Determine the number of tricks won by the given partnership in
 * the given deal
 *
 * \brief tricksWon the TricksWon object for the deal
 * \brief partnership the partnership
 *
 * \return the number of tricks won by partnership in the deal
 */
int getNumberOfTricksWon(const TricksWon& tricksWon, Partnership partnership);

/** \brief Equality operator for TricksWon object
 *
 * \sa TricksWon
 */
bool operator==(const TricksWon&, const TricksWon&);

/** \brief Output TricksWon to stream
 *
 * \param os the output stream
 * \param tricksWon the tricks won to output
 *
 * \return parameter \p os
 */
std::ostream& operator<<(std::ostream& os, const TricksWon& tricksWon);

}

#endif // TRICKSWON_HH_
