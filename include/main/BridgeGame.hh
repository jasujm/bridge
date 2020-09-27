/** \file
 *
 * \brief Definition of Bridge::Main::BridgeGame class
 */

#ifndef MAIN_BRIDGEGAME_HH_
#define MAIN_BRIDGEGAME_HH_

#include "bridge/Call.hh"
#include "bridge/Uuid.hh"
#include "engine/BridgeEngine.hh"
#include "messaging/Identity.hh"
#include "messaging/Sockets.hh"

#include <nlohmann/json.hpp>

#include <memory>
#include <optional>
#include <set>
#include <string>
#include <vector>

namespace Bridge {

class CardType;
class Player;
struct Position;

namespace Messaging {

class CallbackScheduler;

}

namespace Main {

class BridgeGameRecorder;
class CardProtocol;
class PeerCommandSender;

/** \brief Representation of the state of the game
 */
using GameState = nlohmann::json;

/** \brief Single hosted bridge game
 *
 * Each BridgeGame instance glues together an BridgeEngine object, CardProtocol
 * and necessary sockets to communicate with clients and peers. It provides high
 * level interface oriented to handling \ref bridgeprotocolcontrolmessage
 * commands.
 */
class BridgeGame {
public:

    /** \brief Set of positions
     *
     * \sa BridgeGame()
     */
    using PositionSet = std::set<Position>;

    /** \brief Set of user identities
     */
    using IdentitySet = std::set<Messaging::UserId>;

    /** \brief Type of the running counter
     *
     * \sa \ref bridgeprotocolcounter
     */
    using Counter = std::uint64_t;

    /** \brief Create new bridge game
     *
     * The client who creates an instance of BridgeGame is responsible for
     * setting up the infrastructure of the game — namely the card protocols and
     * the peers that will take part in the game. More specifically, this
     * information is provided by instances of CardProtocol and
     * PeerCommandSender. It’s not necessary for the peers to have initiated
     * their handshake and have been accepted.
     *
     * If \p participants is a non‐empty set, all peers added to the game will
     * be matched against the set. Only the known peers are allowed to join the
     * game.
     *
     * \param uuid the UUID of the game
     * \param positionsControlled the positions controlled by the node
     * \param eventSocket ZeroMQ socket used to publish events about the game
     * \param cardProtocol the card protocol used to exchange cards between
     * peers
     * \param peerCommandSender the peer command sender object used to send
     * commands to the peers taking part in the game
     * \param callbackScheduler a callback scheduler object
     * \param participants list of known participants
     * \param recorder A bridge game recorder, if applicable
     * \param engine A pre-constructed bridge engine, if applicable
     */
    BridgeGame(
        const Uuid& uuid,
        PositionSet positionsControlled,
        Messaging::SharedSocket eventSocket,
        std::unique_ptr<CardProtocol> cardProtocol,
        std::shared_ptr<PeerCommandSender> peerCommandSender,
        std::shared_ptr<Messaging::CallbackScheduler> callbackScheduler,
        IdentitySet participants,
        std::shared_ptr<BridgeGameRecorder> recorder = nullptr,
        std::optional<Engine::BridgeEngine> engine = std::nullopt);

    /** \brief Create new bridge game without peers
     *
     * This constructor creates a bridge game without peers. The client who
     * creates a bridge game with this constructor controls all positions.
     *
     * \param uuid the UUID of the game
     * \param eventSocket ZeroMQ socket used to publish events about the game
     * \param cardProtocol the card protocol used to exchange cards between
     * peers
     * \param callbackScheduler a callback scheduler object
     * \param recorder A bridge game recorder, if applicable
     * \param engine A pre-constructed bridge engine, if applicable
     */
    BridgeGame(
        const Uuid& uuid, Messaging::SharedSocket eventSocket,
        std::unique_ptr<CardProtocol> cardProtocol,
        std::shared_ptr<Messaging::CallbackScheduler> callbackScheduler,
        std::shared_ptr<BridgeGameRecorder> recorder = nullptr,
        std::optional<Engine::BridgeEngine> engine = std::nullopt);

    /** \brief Move constructor
     */
    BridgeGame(BridgeGame&&) = default;

    /** \brief Move assignment
     */
    BridgeGame& operator=(BridgeGame&&) = default;

    /** \brief Handle handshake from a peer
     *
     * This method is intended to be used to handle \ref
     * bridgeprotocolcontrolgame command in the bridge protocol. The call
     * returns true if the peer is successfully accepted, or false if not, in
     * which case the call has no effect.
     *
     * If a list of participants was provided for the game, a peer is only
     * allowed to join the game if their user ID is in the participants list.
     *
     * \param identity the identity of the peer
     * \param args arguments for initializing the game (parameter to \ref
     * bridgeprotocolcontrolgame command)
     *
     * \return true if the peer was successfully accepted, false otherwise
     */
    bool addPeer(
        const Messaging::Identity& identity, const nlohmann::json& args);

    /** \brief Get position that a player can join in
     *
     * Determine the position that a node with \p identity can join \p player. A
     * subsequent call to join() from the same node is guaranteed to be
     * successful.
     *
     * If the node is a peer, \p position must be one of the unoccupied
     * positions reserved for the players that the peer represents. If that
     * condition is met, \p position is returned as is. Otherwise none is
     * returned.
     *
     * If the node is a client, \p position may be the preferred position of the
     * player. If \p position is not given, any unoccupied position is selected
     * for the player. If the player cannot be seated in the preferred position,
     * or in case of no preferred position all positions are occupied, none is
     * returned.
     *
     * \param identity the identity of the node
     * \param position the preferred position, if any
     * \param player the player wanting to join the game
     *
     * \return position that the player can join, or none in one of the
     * conditions described earlier
     */
    std::optional<Position> getPositionForPlayerToJoin(
        const Messaging::Identity& identity,
        const std::optional<Position>& position,
        const Player& player) const;

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
     *
     * \return true if it the player was successfully added to the
     * game, false otherwise
     */
    bool join(
        const Messaging::Identity& identity, Position position,
        std::shared_ptr<Player> player);

    /** \brief Get current state of the game
     *
     * This method is intended to implement the \ref
     * bridgeprotocolcontrolget command.
     *
     * \param player The player requesting the state. This affect
     * which card and call/play choice information is available.
     * \param keys The list of keys to retrieve
     *
     * \return Current state of the game visible to \p player
     */
    GameState getState(
        const Player& player,
        const std::optional<std::vector<std::string>>& keys) const;

    /** \brief Get the value of the running counter
     *
     * The running counter is used to synchronize state snapshots to
     * the events published by the BridgeGame instance.
     *
     * \sa \ref bridgeprotocolcounter
     */
    Counter getCounter() const;

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
        const Messaging::Identity& identity, const Player& player,
        const Call& call);

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
        const Messaging::Identity& identity, const Player& player,
        const std::optional<CardType>& card,
        const std::optional<int>& index);

    /** \brief Get non‐owning pointer to peer command sender
     *
     * \return The peer command sender object the game uses, or nullptr if the
     * game is peerless
     */
    PeerCommandSender* getPeerCommandSender();

    /** \brief Get reference to the card protocol
     *
     * \return Reference to the card protocol object the game uses
     */
    CardProtocol& getCardProtocol();

private:

    class Impl;
    std::shared_ptr<Impl> impl;
};

}
}

#endif // MAIN_BRIDGEGAME_HH_
