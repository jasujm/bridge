#ifndef MAIN_BRIDGEGAME_HH_
#define MAIN_BRIDGEGAME_HH_

#include "bridge/Call.hh"
#include "bridge/Uuid.hh"
#include "main/BridgeGameInfo.hh"

#include <boost/core/noncopyable.hpp>
#include <json.hpp>
#include <zmq.hpp>

#include <memory>
#include <optional>
#include <set>
#include <string>
#include <vector>

namespace Bridge {

class CardType;
class Player;
enum class Position;

namespace Engine {

class BridgeEngine;
class DuplicateGameManager;

}

namespace Main {

class CallbackScheduler;
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

    /** \brief Set of positions
     *
     * \sa BridgeGame()
     */
    using PositionSet = std::set<Position>;

    /** \brief Create new bridge game
     *
     * The client who creates an instance of BridgeGame is responsible for
     * setting up the infrastructure of the game — namely the card protocols and
     * the peers that will take part in the game. More specifically, this
     * information is provided by instances of CardProtocol and
     * PeerCommandSender. It’s not necessary for the peers to have initiated
     * their handshake and have been accepted.
     *
     * \param uuid the UUID of the game
     * \param positionsControlled the positions controlled by the node
     * \param eventSocket ZeroMQ socket used to publish events about the game
     * \param cardProtocol the card protocol used to exchange cards between
     * peers
     * \param peerCommandSender the peer command sender object used to send
     * commands to the peers taking part in the game
     * \param callbackScheduler a callback scheduler object
     */
    BridgeGame(
        const Uuid& uuid,
        PositionSet positionsControlled,
        std::shared_ptr<zmq::socket_t> eventSocket,
        std::unique_ptr<CardProtocol> cardProtocol,
        std::shared_ptr<PeerCommandSender> peerCommandSender,
        std::shared_ptr<CallbackScheduler> callbackScheduler);

    /** \brief Create new bridge game without peers
     *
     * This constructor creates a bridge game without peers. The client who
     * creates a bridge game with this constructor controls all positions.
     *
     * \param uuid the UUID of the game
     * \param eventSocket ZeroMQ socket used to publish events about the game
     * \param callbackScheduler a callback scheduler object
     */
    BridgeGame(
        const Uuid& uuid, std::shared_ptr<zmq::socket_t> eventSocket,
        std::shared_ptr<CallbackScheduler> callbackScheduler);

    /** \brief Handle handshake from a peer
     *
     * This method is intended to be used to handle \ref
     * bridgeprotocolcontrolgame command in the bridge protocol. The call
     * returns true if the peer is successfully accepted, or false if not, in
     * which case the call has no effect.
     *
     * \param identity identity of the peer
     * \param args arguments for initializing the game (parameter to \ref
     * bridgeprotocolcontrolgame command)
     *
     * \return true if the peer was successfully accepted, false otherwise
     */
    bool addPeer(const std::string& identity, const nlohmann::json& args);

    /** \brief Get position that a player can join in
     *
     * Determine the position that a node with \p identity can join a player. A
     * subsequent call to join() from the same node is guaranteed to be
     * successful.
     *
     * If the node is a peer, \p position must be one of the unoccupied
     * positions reserved for the players that the peer represents. If that
     * condition is met, \p position is returned as is. Otherwise none is
     * returned.
     *
     * If the node is a client, \p position may be the preferred position of the
     * player. If \p position is none, any unoccupied position is selected for
     * the player. If the player cannot be seated in the preferred position, or
     * in case of no preferred position all positions are occupied, none is
     * returned.
     *
     * \param identity the identity of the node
     * \param position the preferred position, if any
     *
     * \return position that the player can join, or none in one of the
     * conditions described earlier
     */
    std::optional<Position> getPositionForPlayerToJoin(
        const std::string& identity, const std::optional<Position>& position);

    /** \brief Join a player in the game
     *
     * This method is intended to implement the \ref bridgeprotocolcontroljoin
     * command. If called with \p identity and \p position earlier returned by a
     * call to getPositionForPlayerToJoin(), \p player controlled by the node is
     * seated in that position.
     *
     * \pre
     * - Position must be a valid position previously returned by
     *   getPositionForPlayerToJoin()
     * - \p player must not be nullptr
     *
     * \param identity the identity of the node controlling the player
     * \param position the position the player is seated in
     * \param player the player to join the game
     */
    void join(
        const std::string& identity, Position position,
        std::shared_ptr<Player> player);

    /** \brief Make call
     *
     * This method is intended to implement the \ref bridgeprotocolcontrolcall
     * command.
     *
     * \param identity the identity of the node controlling the player
     * \param player the player making the call
     * \param call the call to be made
     *
     * \return true if the call was successful, false otherwise
     */
    bool call(
        const std::string& identity, const Player& player, const Call& call);

    /** \brief Play a card
     *
     * This method is intended to implement the \ref bridgeprotocolcontrolplay
     * command. Exactly one of \p card or \p index must be defined.
     *
     * \param identity the identity of the node controlling the player
     * \param player the player playing the card
     * \param card the type of the card to be played (optional)
     * \param index the index of the card to be played (optional)
     *
     * \return true if the call was successful, false otherwise
     */
    bool play(
        const std::string& identity, const Player& player,
        const std::optional<CardType>& card,
        const std::optional<std::size_t>& index);

private:

    const Engine::BridgeEngine& handleGetEngine() const override;

    const Engine::DuplicateGameManager& handleGetGameManager() const override;

    class Impl;
    std::shared_ptr<Impl> impl;
};

}
}

#endif // MAIN_BRIDGEGAME_HH_
