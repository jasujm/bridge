#ifndef MAIN_BRIDGEGAME_HH_
#define MAIN_BRIDGEGAME_HH_

#include "bridge/Call.hh"
#include "bridge/Position.hh"
#include "main/BridgeGameInfo.hh"

#include <boost/core/noncopyable.hpp>
#include <boost/optional/optional_fwd.hpp>
#include <json.hpp>
#include <zmq.hpp>

#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

// TODO: Only neded for sendToPeers() and sendToPeersIfClient() definitions.
#include <algorithm>
#include <cassert>
#include "main/PeerCommandSender.hh"
#include "messaging/JsonSerializer.hh"

namespace Bridge {

class CardType;
class Player;

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

    using EndpointVector = std::vector<std::string>;
    using PositionVector = std::vector<Position>;
    using PositionSet = std::set<Position>;
    using VersionVector = std::vector<int>;

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

    void startIfReady();

    template<typename... Args>
    void sendToPeers(const std::string& command, Args&&... args);

    template<typename... Args>
    void sendToPeersIfClient(
        const std::string& identity, const std::string& command,
        Args&&... args);

    bool call(
        const std::string& identity, const Player& player, const Call& call);
    bool play(
        const std::string& identity, const Player& player,
        const boost::optional<CardType>& card,
        const boost::optional<std::size_t>& index);

    std::shared_ptr<Engine::DuplicateGameManager> gameManager;
    std::map<std::string, PositionVector> peers;
    std::set<std::string> clients;
    const PositionSet positionsControlled;
    PositionSet positionsInUse;
    std::shared_ptr<PeerCommandSender> peerCommandSender;
    std::unique_ptr<CardProtocol> cardProtocol;
    std::shared_ptr<Engine::BridgeEngine> engine;
    std::shared_ptr<zmq::socket_t> eventSocket;

    class Impl;
    std::shared_ptr<Impl> impl;
};

// TODO: Currently accessed both from BridgeMain and BridgeGame. Definition
// could be removed from header once the dependency is removed.

template<typename... Args>
void BridgeGame::sendToPeers(
    const std::string& command, Args&&... args)
{
    assert(peerCommandSender);
    peerCommandSender->sendCommand(
        Messaging::JsonSerializer {}, command, std::forward<Args>(args)...);
}

template<typename... Args>
void BridgeGame::sendToPeersIfClient(
    const std::string& identity, const std::string& command, Args&&... args)
{
    if (clients.find(identity) != clients.end()) {
        sendToPeers(command, std::forward<Args>(args)...);
    }
}

}
}

#endif // MAIN_BRIDGEGAME_HH_
