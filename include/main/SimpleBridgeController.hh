/** \file
 *
 * \brief Definition of Bridge::Main::SimpleBridgeController class
 */

#ifndef MAIN_SIMPLEBRIDGECONTROLLER_HH_
#define MAIN_SIMPLEBRIDGECONTROLLER_HH_

#include "engine/BridgeEngine.hh"
#include "main/BridgeController.hh"
#include "Utility.hh"

#include <boost/core/noncopyable.hpp>

#include <memory>
#include <vector>

namespace Bridge {

class DealState;
class Player;

namespace Engine {
class CardManager;
class GameManager;
}

/** \brief Services for the main game component
 *
 * Namespace Main contains the classes used by the main bridge thread to
 * handle all tasks related to the bridge application.
 */
namespace Main {

/** \brief BridgeController implementation for local games
 *
 * SimpleBridgeController is a simple implementation of BridgeController
 * interface suitable for local games. When receiving commands, it assumes
 * that it is coming from the player who has turn. SimpleBridgeController has
 * no notion of different bridge application instances controlling particular
 * players. Each command is unconditionally accepted and applied to the
 * underlying BridgeEngine.
 */
class SimpleBridgeController : public BridgeController,
                               private boost::noncopyable {
public:

    /** \brief Create simple bridge controller
     *
     * The parameters of this method will be used to create the BridgeEngine
     * for the SimpleBridgeController object.
     *
     * \tparam PlayerIterator an input iterator which must return a shared_ptr
     * to a player when dereferenced
     *
     * \param cardManager the card manager
     * \param gameManager the game manager
     * \param firstPlayer the first player
     * \param lastPlayer the last player
     *
     * \sa Bridge::Engine::BridgeEngine::BridgeEngine
     */
    template<typename PlayerIterator>
    SimpleBridgeController(
        std::shared_ptr<Engine::CardManager> cardManager,
        std::shared_ptr<Engine::GameManager> gameManager,
        PlayerIterator firstPlayer, PlayerIterator lastPlayer);

    /** \brief Determine deal state
     *
     * \return DealState object describing the current deal state
     */
    DealState getState() const;

private:

    void handleCall(const Call& call) override;

    void handlePlay(const CardType& cardType) override;

    const std::unique_ptr<Engine::BridgeEngine> engine;
};

template<typename PlayerIterator>
SimpleBridgeController::SimpleBridgeController(
    std::shared_ptr<Engine::CardManager> cardManager,
    std::shared_ptr<Engine::GameManager> gameManager,
    PlayerIterator firstPlayer, PlayerIterator lastPlayer) :
    engine {
        std::make_unique<Engine::BridgeEngine>(
            std::move(cardManager), std::move(gameManager),
            firstPlayer, lastPlayer)}
{
}

}
}

#endif // MAIN_SIMPLEBRIDGECONTROLLER_HH_
