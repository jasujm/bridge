/** \file
 *
 * \brief Definition of the DuplicateScore struct
 */

#ifndef SCORING_DUPLICATESCORE_HH_
#define SCORING_DUPLICATESCORE_HH_

#include "bridge/Partnership.hh"

#include <boost/operators.hpp>

#include <iosfwd>

namespace Bridge {

class Contract;

namespace Scoring {

/** \brief Duplicate bridge score entry
 */
struct DuplicateScore : private boost::equality_comparable<DuplicateScore> {
    Partnership partnership;  ///< \brief The partnership scoring
    int score;                ///< \brief The amount of score

    DuplicateScore() = default;

    /** \brief Create new duplicate score
     *
     * \param partnership see \ref partnership
     * \param score see \ref score
     */
    constexpr DuplicateScore(Partnership partnership, int score) :
        partnership {partnership},
        score {score}
    {
    }
};

/** \brief Equality operator for duplicate scores
 *
 * \sa DuplicateScore
 */
bool operator==(const DuplicateScore&, const DuplicateScore&);

/** \brief Output a DuplicateScore to stream
 *
 * \param os the output stream
 * \param score the score to output
 *
 * \return parameter \p os
 */
std::ostream& operator<<(std::ostream& os, const DuplicateScore& score);

}
}

#endif // SCORING_DUPLICATESCORING_HH_
