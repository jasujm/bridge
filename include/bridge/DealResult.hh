/** \file
 *
 * \brief Definition of Bridge::DealResult struct
 */

#ifndef DEALRESULT_HH_
#define DEALRESULT_HH_

#include "BridgeConstants.hh"

#include <boost/operators.hpp>

#include <stdexcept>

namespace Bridge {

enum class Partnership;

/** \brief The result of a (possible ongoing) bridge deal
 *
 * DealResult objects are equality comparable. They compare equal when tricks
 * won by each partnership are equal.
 *
 * \note Boost operators library is used to ensure that operator!= is
 * generated with usual semantics when operator== is supplied.
 */
struct DealResult : private boost::equality_comparable<DealResult> {

    /** \brief The number of tricks won by the north–south partnership
     */
    int tricksWonByNorthSouth;

    /** \brief The number of tricks won by the east–west partnership
     */
    int tricksWonByEastWest;

    /** \brief Create new deal result
     *
     * \param tricksWonByNorthSouth the number of tricks won by the
     * north–south partnership
     * \param tricksWonByEastWest the number of tricks won by the east–west
     * partnership
     *
     * \throw std::invalid_argument if either parameter is negative or their
     * sum exceeds number of tricks in a deal
     */
    constexpr DealResult(int tricksWonByNorthSouth, int tricksWonByEastWest) :
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
 * \brief dealResult the deal result
 * \brief partnership the partnership
 *
 * \return the number of tricks won by partnership in the deal
 */
int getNumberOfTricksWon(const DealResult& dealResult, Partnership partnership);

/** \brief Equality operator for deal results
 *
 * \sa DealResult
 */
bool operator==(const DealResult&, const DealResult&);

}

#endif // DEALRESULT_HH_
