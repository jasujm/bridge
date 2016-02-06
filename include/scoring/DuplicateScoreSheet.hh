/** \file
 *
 * \brief Definition of Bridge::Scoring::DuplicateScoreSheet class
 */

#ifndef SCORING_DUPLICATESCORESHEET_HH_
#define SCORING_DUPLICATESCORESHEET_HH_

#include "bridge/Partnership.hh"

#include <boost/operators.hpp>
#include <boost/optional/optional.hpp>

#include <initializer_list>
#include <iosfwd>
#include <stdexcept>
#include <vector>

namespace Bridge {

class Contract;
class Vulnerability;

namespace Scoring {

/** \brief Score sheet in duplicate scoring
 *
 * DuplicateScoreSheet admits results of duplicate bridge deals and produces
 * score entries.
 */
class DuplicateScoreSheet {
public:

    /** \brief An score in duplicate score sheet
     *
     * Score object represents score for a finished (non passed‐out)
     * deal. Score objects are equality comparable. They compare equal when
     * both partnership and score are equal.
     *
     * \note Boost operators library is used to ensure that operator!= is
     * generated with usual semantics when operator== is supplied.
     */
    struct Score : private boost::equality_comparable<Score> {
        Partnership partnership; ///< \brief The partnership that scored
        int score; ///< \brief The score

        /** \brief Create new duplicate score entry
         *
         * \param partnership the partnership that scored
         * \param score the amount scored
         *
         * \throw std::invalid_argument if score <= 0
         */
        constexpr Score(Partnership partnership, int score) :
            partnership {partnership},
            score {(score > 0) ? score :
                    throw std::invalid_argument("Non-positive score")}
        {
        }
    };

    /** \brief An entry in duplicate score sheet
     *
     * Entries in score sheet are optional Score object. They contain value if
     * the corresponding deal was finished. If the deal was passed out, entry
     * contains no value.
     */
    using Entry = boost::optional<Score>;

    /** \brief Create duplicate score sheet that is initially empty
     */
    DuplicateScoreSheet();

    /** \brief Create duplicate score sheet that initially contains some entries
     *
     * \tparam EntryIterator An input iterator that, when dereferenced,
     * returns element convertible to optional Score.
     *
     * \param first iterator to the first entry
     * \param last iterator to the last entry
     */
    template<typename EntryIterator>
    DuplicateScoreSheet(EntryIterator first, EntryIterator last);

    /** \brief Create duplicate score sheet that initially contains some entries
     *
     * \param entries initializer list containing initial entries
     */
    DuplicateScoreSheet(std::initializer_list<Entry> entries);

    /** \brief Add result from a deal
     *
     * \param partnership partnership the contract belongs to
     * \param contract contract in the last deal
     * \param tricksWon number of tricks the partnership won
     * \param vulnerability the vulnerability status for the deal
     */
    void addResult(
        Partnership partnership, const Contract& contract, int tricksWon,
        const Vulnerability& vulnerability);

    /** \brief Add passed out deal
     */
    void addPassedOut();

    /** \brief Retrieve iterator to the beginning of the scoresheet
     *
     * \return The iterator returned by this function is a random access
     * iterator that, when dereferenced, return a contant reference to an
     * Entry object.
     */
    auto begin() const;

    /** \brief Retrieve iterator to the end of the scoresheet
     *
     * \copydetails begin()
     */
    auto end() const;

private:

    std::vector<Entry> entries;
};

template<typename EntryIterator>
DuplicateScoreSheet::DuplicateScoreSheet(
    EntryIterator first, EntryIterator last) : entries(first, last)
{
}

inline auto DuplicateScoreSheet::begin() const
{
    return entries.begin();
}

inline auto DuplicateScoreSheet::end() const
{
    return entries.end();
}

/** \brief Equality operator for duplicate score sheet
 *
 * \sa DuplicateScoreSheet
 */
bool operator==(const DuplicateScoreSheet&, const DuplicateScoreSheet&);

/** \brief Equality operator for duplicate score sheet scores
 *
 * \sa DuplicateScoreSheet::Score
 */
bool operator==(
    const DuplicateScoreSheet::Score&, const DuplicateScoreSheet::Score&);

/** \brief Output a duplicate score sheet to stream
 *
 * \param os the output stream
 * \param scoreSheet the score sheet to output
 *
 * \return parameter \p os
 */
std::ostream& operator<<(
    std::ostream& os, const DuplicateScoreSheet& scoreSheet);

/** \brief Output a duplicate score sheet score to stream
 *
 * \param os the output stream
 * \param score the score to output
 *
 * \return parameter \p os
 */
std::ostream& operator<<(
    std::ostream& os, const DuplicateScoreSheet::Score& score);

}
}

#endif // SCORING_DUPLICATESCORESHEET_HH_
