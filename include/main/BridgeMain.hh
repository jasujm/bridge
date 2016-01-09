/** \file
 *
 * \brief Definition of Bridge::Main::BridgeMain class
 */

#ifndef MAIN_BRIDGEMAIN_HH_
#define MAIN_BRIDGEMAIN_HH_

#include "engine/BridgeEngine.hh"
#include "main/BridgeController.hh"
#include "Utility.hh"

#include <boost/core/noncopyable.hpp>

#include <memory>
#include <vector>

namespace Bridge {

class GameState;
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

/** \brief Main class that integrates BridgeEngine and its dependencies
 *
 * BridgeMain is the brain of the bridge application used by the main bridge
 * thread. It has the main responsibility of managing the business logic and
 * other services such as protocols etc.
 */
class BridgeMain : public BridgeController, private boost::noncopyable {
public:

    /** \brief Create bridge main
     *
     * The parameters of this method will be used to create the BridgeEngine
     * for the BridgeMain object.
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
    BridgeMain(
        std::shared_ptr<Engine::CardManager> cardManager,
        std::shared_ptr<Engine::GameManager> gameManager,
        PlayerIterator firstPlayer, PlayerIterator lastPlayer);

    /** \brief Determine game state
     *
     * \return GameState object describing the current game state
     */
    GameState getState() const;

private:

    void handleCall(const Call& call) override;

    void handlePlay(const CardType& cardType) override;

    Player* internalGetPlayer();

    const std::unique_ptr<Engine::BridgeEngine> engine;
    const std::vector<std::shared_ptr<Player>> players;
};

template<typename PlayerIterator>
BridgeMain::BridgeMain(
    std::shared_ptr<Engine::CardManager> cardManager,
    std::shared_ptr<Engine::GameManager> gameManager,
    PlayerIterator firstPlayer, PlayerIterator lastPlayer) :
    engine {
        std::make_unique<Engine::BridgeEngine>(
            std::move(cardManager), std::move(gameManager),
            firstPlayer, lastPlayer)},
    players(firstPlayer, lastPlayer)
{
}

}
}

#endif // MAIN_BRIDGEMAIN_HH_
