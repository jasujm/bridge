/** \file
 *
 * \brief Definition of Bridge::Main::BridgeMain class
 */

#ifndef MAIN_BRIDGEMAIN_HH_
#define MAIN_BRIDGEMAIN_HH_

#include "bridge/BridgeGame.hh"
#include "engine/BridgeEngine.hh"
#include "Observer.hh"
#include "Utility.hh"

#include <boost/core/noncopyable.hpp>

#include <functional>
#include <memory>
#include <queue>
#include <vector>

namespace Bridge {

class Player;

namespace Engine {
class CardManager;
class GameManager;
struct Shuffled;
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
class BridgeMain : public BridgeGame,
                   public Observer<Engine::Shuffled>,
                   public std::enable_shared_from_this<BridgeMain>,
                   private boost::noncopyable {
public:

    /** \brief Event queue used by BridgeMain
     *
     * \todo This is a temporary solution used to deal with non-re-entrancy of
     * the Statechart library used to implement BridgeEngine. It is not thread
     * safe and thus should be remove once obsolete.
     */
    using EventQueue = std::queue<std::function<void()>>;

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
    static std::shared_ptr<BridgeMain> makeBridgeMain(
        std::shared_ptr<Engine::CardManager> cardManager,
        std::shared_ptr<Engine::GameManager> gameManager,
        PlayerIterator firstPlayer, PlayerIterator lastPlayer);

private:

    BridgeMain(
        std::unique_ptr<Engine::BridgeEngine> engine,
        std::vector<std::shared_ptr<Player>> players);

    void init(Engine::CardManager& cardManager);

    void handleCall(const Call& call) override;

    void handlePlay(const CardType& cardType) override;

    GameState handleGetState() const override;

    void handleNotify(const Engine::Shuffled&) override;

    Player* internalGetPlayer();

    const std::unique_ptr<Engine::BridgeEngine> engine;
    const std::vector<std::shared_ptr<Player>> players;
    EventQueue events;
};

template<typename PlayerIterator>
std::shared_ptr<BridgeMain> BridgeMain::makeBridgeMain(
    std::shared_ptr<Engine::CardManager> cardManager,
    std::shared_ptr<Engine::GameManager> gameManager,
    PlayerIterator firstPlayer, PlayerIterator lastPlayer)
{
    auto& card_manager = dereference(cardManager);
    auto engine = std::make_unique<Engine::BridgeEngine>(
        std::move(cardManager), std::move(gameManager),
        firstPlayer, lastPlayer);
    // Can't use make_shared because because constructor is private
    auto main = std::shared_ptr<BridgeMain>{
        new BridgeMain {
            std::move(engine),
            std::vector<std::shared_ptr<Player>>(firstPlayer, lastPlayer)}};
    main->init(card_manager);
    return main;
}

}
}

#endif // MAIN_BRIDGEMAIN_HH_
