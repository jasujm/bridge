/** \file
 *
 * \brief Definition of an utility to calculate duplicate bridge scores
 */

#ifndef SCORING_DUPLICATESCORING_HH_
#define SCORING_DUPLICATESCORING_HH_

#include <utility>

namespace Bridge {

class Contract;

/** \brief Services related to duplicate bridge scoring
 */
namespace Scoring {

/** \brief Calculate score according to duplicate bridge rules
 *
 * \param contract the contract of the deal
 * \param vulnerable is the declarer vulnerable
 * \param tricksWon the number of tricks won by the declarer
 *
 * \return A pair containing two elements:
 *   - \c bool indicating whether the partnership made the contract
 *   - \c int indicating the score won by the declarer if the contract was made,
 *     or the opponent if the contract was defeated
 */
std::pair<bool, int> calculateDuplicateScore(
    const Contract& contract, bool vulnerable, int tricksWon);

}
}

#endif // SCORING_DUPLICATESCORING_HH_
