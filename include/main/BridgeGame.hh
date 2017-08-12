#ifndef MAIN_BRIDGEGAME_HH_
#define MAIN_BRIDGEGAME_HH_

#include "bridge/Position.hh"
#include "main/BridgeGameInfo.hh"

#include <boost/core/noncopyable.hpp>
#include <zmq.hpp>

#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

namespace Bridge {

namespace Engine {

class BridgeEngine;
class DuplicateGameManager;

}

namespace Main {

class CardProtocol;
class PeerCommandSender;

/** \brief Single hosted bridge game
 *
 * An instance of BridgeGame contains all information required to host a bridge
 * game, including state of the game and list of all nodes taking part in the
 * game.
 */
class BridgeGame : public BridgeGameInfo, private boost::noncopyable {
public:

    using PositionVector = std::vector<Position>;
    using PositionSet = std::set<Position>;

    BridgeGame(
        PositionSet positionsControlled,
        std::shared_ptr<zmq::socket_t> eventSocket);

    /** \brief Initialize card protocol
     *
     * \todo Ideally BridgeGame shouldn’t rely on two‐step
     * initialization. However, there are currently two way dependencies between
     * message handlers and game object requiring this.
     */
    void initializeCardProtocol(std::unique_ptr<CardProtocol> cardProtocol);

//private:

    const Engine::BridgeEngine& handleGetEngine() const override;

    const Engine::DuplicateGameManager& handleGetGameManager() const override;

    std::shared_ptr<Engine::DuplicateGameManager> gameManager;
    std::map<std::string, PositionVector> peers;
    std::set<std::string> clients;
    const PositionSet positionsControlled;
    PositionSet positionsInUse;
    std::shared_ptr<PeerCommandSender> peerCommandSender;
    std::unique_ptr<CardProtocol> cardProtocol;
    std::shared_ptr<Engine::BridgeEngine> engine;
    std::shared_ptr<zmq::socket_t> eventSocket;
};

}
}

#endif // MAIN_BRIDGEGAME_HH_
