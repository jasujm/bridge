/** \file
 *
 * \brief Definition of an utility to calculate duplicate bridge scores
 */

#ifndef DUPLICATESCORING_HH_
#define DUPLICATESCORING_HH_

#include <iosfwd>
#include <optional>

#include "bridge/Partnership.hh"

namespace Bridge {

class Contract;

/** \brief Results of a duplicate bridge deal
 */
struct DuplicateResult {

    /** \brief Default constructor
     *
     * Create duplicate result where no score is awarded (passed out deal).
     */
    constexpr DuplicateResult() :
        partnership {},
        score {}
    {
    }

    /** \brief Create new duplicate result
     *
     * \param partnership The partnership awarded the score
     * \param score The score
     */
    constexpr DuplicateResult(Partnership partnership, int score) :
        partnership {partnership},
        score {score}
    {
    }

    /** \brief Create passed out duplicate result
     */
    static constexpr DuplicateResult passedOut()
    {
        return {};
    }

    /** \brief The partnership awarded score
     *
     * This member specifies the partnership that was awarded the score in a
     * deal, or none if the deal passed out.
     */
    std::optional<Partnership> partnership;

    /** \brief The score awarded to \ref partnership
     */
    int score;
};

/** \brief Calculate score according to the duplicate bridge rules
 *
 * \param contract the contract of the deal
 * \param vulnerable is the declarer vulnerable
 * \param tricksWon the number of tricks won by the declarer
 *
 * \return The score for the declarer. If the declarer made the contract, the
 * score is positive. If the contract was defeated, the score is negative, and
 * awarded to the opponent.
 */
int calculateDuplicateScore(
    const Contract& contract, bool vulnerable, int tricksWon);

/** \brief Make duplicate result from score
 *
 * This function can be used to convert a signed score to a DuplicateResult
 * object. If \p score is positive, the score is awarded to \p partnership. If
 * negative, the negation is awarded to the opponent. If \p score is nullopt, a
 * default DuplicateResult (corresponding to passed out deal) is returned.
 *
 * \param partnership The declarer partnership
 * \param score The score
 *
 * \return Duplicate result corresponding to \p partnership and \p score
 */
DuplicateResult makeDuplicateResult(Partnership partnership, int score);

/** \brief Equality operator for duplicate results
 */
bool operator==(const DuplicateResult& lhs, const DuplicateResult& rhs);

/** \brief Output a duplicate result to stream
 *
 * \param os The output stream
 * \param result The deal result
 */
std::ostream& operator<<(std::ostream& os, const DuplicateResult& result);

}

#endif // DUPLICATESCORING_HH_
