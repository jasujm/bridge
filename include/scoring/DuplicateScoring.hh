/** \file
 *
 * \brief Definition of an utility to calculate duplicate bridge scores
 */

#ifndef SCORING_DUPLICATESCORING_HH_
#define SCORING_DUPLICATESCORING_HH_

namespace Bridge {

class Contract;
struct Partnership;

/** \brief Services related to duplicate bridge scoring
 */
namespace Scoring {

struct DuplicateScore;

/** \brief Calculate score according to the duplicate bridge rules
 *
 * \param partnership the partnership owning the contract
 * \param contract the contract of the deal
 * \param vulnerable is the declarer vulnerable
 * \param tricksWon the number of tricks won by the declarer
 *
 * \return The outcome of the deal given the parameters according to the
 * duplicate scoring rules. For a contract bid and made, the score is awarded to
 * the partnership owning the contract. For a contract not made, the score is
 * awarded to the opponents.
 */
DuplicateScore calculateDuplicateScore(
    Partnership partnership, const Contract& contract, bool vulnerable,
    int tricksWon);

}
}

#endif // SCORING_DUPLICATESCORING_HH_
