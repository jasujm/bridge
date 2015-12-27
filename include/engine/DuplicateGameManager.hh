/** \file
 *
 * \brief Definition of Bridge::Engine::DuplicateGameManager class
 */

#ifndef ENGINE_DUPLICATEGAMEMANAGER_HH_
#define ENGINE_DUPLICATEGAMEMANAGER_HH_

#include "bridge/Partnership.hh"
#include "engine/GameManager.hh"

#include <boost/core/noncopyable.hpp>
#include <boost/operators.hpp>
#include <boost/optional/optional.hpp>
#include <boost/range/iterator_range.hpp>

#include <cstddef>
#include <stdexcept>
#include <vector>

namespace Bridge {
namespace Engine {

/** \brief GameManager that applies duplicate bridge scoring
 *
 * This class is meant to be used for social bridge games. Although named
 * duplicate, it is used to simulate a single game with no rotation. The
 * duplicate refers to duplicate scoring.
 */
class DuplicateGameManager : public GameManager, private boost::noncopyable {
public:

    /** \brief An entry in duplicate scoresheet
     *
     * ScoreEntry objects are equality comparable. They compare equal when
     * both partnership and score are equal.
     *
     * \note Boost operators library is used to ensure that operator!= is
     * generated with usual semantics when operator== is supplied.
     */
    struct ScoreEntry : private boost::equality_comparable<ScoreEntry> {
        /** \brief The partnership the score belongs to
         */
        Partnership partnership;
        int score; ///< \brief The score

        /** \brief Create new duplicate score entry
         *
         * \param partnership the partnership that scored
         * \param score the amount scored
         *
         * \throw std::invalid_argument if score <= 0
         */
        constexpr ScoreEntry(Partnership partnership, int score) :
            partnership {partnership},
            score {(score > 0) ? score :
                    throw std::invalid_argument("Non-positive score")}
        {
        }
    };

    /** \brief Determine the number of enties in the scoresheet
     *
     * \return the number of entries in the game
     */
    std::size_t getNumberOfEntries() const;

    /** \brief Retrieve an entry from the scoresheet
     *
     * \param n the index of the entry
     *
     * \return the entry
     *
     * \throw std::out_of_range if n >= getNumberOfEntries()
     */
    boost::optional<ScoreEntry> getScoreEntry(std::size_t n) const;

    /** \brief Retrieve iterator to the beginning of score entries
     *
     * \sa getScoreEntry()
     */
    auto beginScoreEntries() const;

    /** \brief Retrieve iterator to the end of score entries
     *
     * \sa getScoreEntry()
     */
    auto endScoreEntries() const;

private:

    void handleAddResult(Partnership partnership, const Contract& contract,
                         int tricksWon) override;

    void handleAddPassedOut() override;

    bool handleHasEnded() const override;

    Position handleGetOpenerPosition() const override;

    Vulnerability handleGetVulnerability() const override;

    std::vector<boost::optional<ScoreEntry>> entries;
};

inline auto DuplicateGameManager::beginScoreEntries() const
{
    return entries.begin();
}

inline auto DuplicateGameManager::endScoreEntries() const
{
    return entries.end();
}

/** \brief Iterator range over all score entries for a gameBonus
 *
 * \param gameManager the game manager from which the entries are extracted
 *
 * \return an iterator range over all sore entries in the game
 */
inline auto getScoreEntries(const DuplicateGameManager& gameManager)
{
    return boost::make_iterator_range(
        gameManager.beginScoreEntries(), gameManager.endScoreEntries());
}

/** \brief Equality operator for duplicate score entries
 *
 * \sa DuplicateGameManager::ScoreEntry
 */
bool operator==(const DuplicateGameManager::ScoreEntry&,
                const DuplicateGameManager::ScoreEntry&);

}
}

#endif // ENGINE_DUPLICATEGAMEMANAGER_HH_
