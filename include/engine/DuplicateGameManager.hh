/** \file
 *
 * \brief Definition of Bridge::Engine::DuplicateGameManager class
 */

#ifndef ENGINE_DUPLICATEGAMEMANAGER_HH_
#define ENGINE_DUPLICATEGAMEMANAGER_HH_

#include "bridge/Partnership.hh"
#include "engine/GameManager.hh"

#include <boost/core/noncopyable.hpp>

#include <optional>

namespace Bridge {
namespace Engine {

/** \brief GameManager that applies duplicate bridge scoring
 *
 * This class is meant to be used for social bridge games. Although named
 * duplicate, it is used to simulate a single game with no rotation. The
 * duplicate refers to duplicate scoring.
 *
 * Duplicate game manager does not store the state of multiple deals. It returns
 * a DuplicateResult object when the result is added or deal is passed out and
 * immediately forgets it.
 */
class DuplicateGameManager : public GameManager, private boost::noncopyable {
public:

    /** \brief Create new duplicate game manager
     */
    DuplicateGameManager();

    /** \brief Create new duplicate game manager with initial state
     *
     * \param openerPosition initial opener position
     * \param vulnerability initial vulnerability
     *
     * \throw std::invalid_argument if \p openerPosition or \p vulnerability is
     * invalid
     */
    DuplicateGameManager(
        Position openerPosition, Vulnerability vulnerability);

private:

    ResultType handleAddResult(
        Partnership partnership, const Contract& contract,
        int tricksWon) override;

    ResultType handleAddPassedOut() override;

    bool handleHasEnded() const override;

    Position handleGetOpenerPosition() const override;

    Vulnerability handleGetVulnerability() const override;

    int round;
};

}
}

#endif // ENGINE_DUPLICATEGAMEMANAGER_HH_
