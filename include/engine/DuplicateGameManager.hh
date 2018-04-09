/** \file
 *
 * \brief Definition of Bridge::Engine::DuplicateGameManager class
 */

#ifndef ENGINE_DUPLICATEGAMEMANAGER_HH_
#define ENGINE_DUPLICATEGAMEMANAGER_HH_

#include "engine/GameManager.hh"
#include "scoring/DuplicateScoreSheet.hh"

#include <boost/core/noncopyable.hpp>

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

    /** \brief Retrieve the score sheet for the game
     */
    const Scoring::DuplicateScoreSheet& getScoreSheet() const;

private:

    boost::any handleAddResult(
        Partnership partnership, const Contract& contract,
        int tricksWon) override;

    boost::any handleAddPassedOut() override;

    bool handleHasEnded() const override;

    Position handleGetOpenerPosition() const override;

    Vulnerability handleGetVulnerability() const override;

    Scoring::DuplicateScoreSheet scoreSheet;
};

}
}

#endif // ENGINE_DUPLICATEGAMEMANAGER_HH_
