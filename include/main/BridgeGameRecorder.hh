/** \file
 *
 * \brief Definition of Bridge::Main::BridgeGameRecorder
 */

#ifndef MAIN_BRIDGEGAMERECORDER_HH_
#define MAIN_BRIDGEGAMERECORDER_HH_

#include "bridge/DuplicateScoring.hh"
#include "bridge/Call.hh"
#include "bridge/Uuid.hh"
#include "messaging/Identity.hh"

#include <memory>
#include <optional>
#include <string_view>
#include <vector>

namespace Bridge {

namespace Engine {
class GameManager;
}

class CardType;
class Deal;
class Position;
class Trick;

namespace Main {

class CardProtocol;

/** \brief Persistence for bridge games
 *
 * A BridgeGameRecorder listens to events from bridge games and records the
 * deals to a database. It allows recalling a deal from the recorded data.
 *
 * The recorder is only compiled if WITH_RECORDER macro is defined with true
 * value. Otherwise a stub implementing this interface is compiled.
 *
 * \todo Use rocksdb merge operations to record individual calls, cards,
 * etc. smarter
 */
class BridgeGameRecorder {
public:

    /** \brief Object encapsulating deal state
     *
     * The members of a deal state can be used to construct a new bridge engine
     * continuing from the state the deal was when last recorded.
     */
    struct DealState {
        /// The current deal
        std::unique_ptr<Deal> deal;
        /// %Card protocol encapsulating the cards in the current deal
        std::unique_ptr<CardProtocol> cardProtocol;
        /// Game manager encapsulating the state of the game
        std::shared_ptr<Engine::GameManager> gameManager;
    };

    /** \brief Object encapsulating game state
     */
    struct GameState {
        /// The UUIDs of the players in the game
        std::optional<Uuid> playerUuids[4];
        /// The UUID of the current deal in the game
        std::optional<Uuid> dealUuid;
    };

    /** \brief Object representing a deal result
     */
    struct DealResult {
        /// The UUID of the deal
        Uuid dealUuid;
        /// The deal result, or nullopt if the deal is not completed
        std::optional<DuplicateResult> result;
    };

    /** \brief Create new bridge game recorder
     *
     * \param path Path to the database
     */
    BridgeGameRecorder(const std::string_view path);

    virtual ~BridgeGameRecorder();

    /** \brief Record the state of a game
     *
     * \param gameUuid The UUID of the game to record
     * \param gameState The game state
     */
    void recordGame(const Uuid& gameUuid, const GameState& gameState);

    /** \brief Record the state of \p deal
     *
     * \param deal the deal to record
     */
    void recordDeal(const Deal& deal);

    /** \brief Record a call
     *
     * This method will record a single \p call in a deal specified by \p
     * dealUuid. It relies on merge operations and is faster than recordDeal()
     * but assumes that the deal is in a state where the given call is allowed.
     *
     * \param dealUuid The UUID of the deal
     * \param call The call to record
     */
    void recordCall(const Uuid& dealUuid, const Call& call);

    /** \brief Record a trick
     *
     * This method will record a new trick that will be lead by the player at \p
     * leaderPosition. The cards played to the trick are then recorded using
     * recordCard(). It relies on merge operations and is faster than
     * recordDeal() but assumes that the deal is in a state where the given
     * trick is allowed.
     *
     * \param dealUuid The UUID of the deal
     * \param leaderPosition The position of the player leading to the trick
     */
    void recordTrick(const Uuid& dealUuid, Position leaderPosition);

    /** \brief Record a card played to a trick
     *
     * This method will record the \p card given as parameter played to the
     * trick previously recorded using recordTrick(). It relies on merge
     * operations and is faster than recordDeal() but assumes that the deal is
     * in a state where the given card can be played.
     *
     * \param dealUuid The UUID of the deal
     * \param card The type of the card played to the trick
     */
    void recordCard(const Uuid& dealUuid, const CardType& card);

    /** \brief Record information \p player
     *
     * \param playerUuid The UUID of the player to record
     * \param userId The ID of the user controlling the player
     */
    void recordPlayer(const Uuid& playerUuid, Messaging::UserIdView userId);

    /** \brief Record information about a started deal
     *
     * \param gameUuid The UUID of the game
     * \param dealUuid The UUID of the newly started deal
     */
    void recordDealStarted(const Uuid& gameUuid, const Uuid& dealUuid);

    /** \brief Record information about an ended deal
     *
     * \param gameUuid The UUID of the game
     * \param result The deal result
     */
    void recordDealEnded(const Uuid& gameUuid, const DuplicateResult& result);

    /** \brief Retrieve a game from the database
     *
     * \param gameUuid The UUID of the game to recall
     *
     * \return UUID of the current deal, or nullopt if the game is not known
     */
    std::optional<GameState> recallGame(const Uuid& gameUuid);

    /** \brief Retrieve a deal from the database
     *
     * \param dealUuid The UUID of the deal to recall
     *
     * \return The recalled deal, or nullptr if the deal is not known
     */
    std::optional<DealState> recallDeal(const Uuid& dealUuid);

    /** \brief Retrieve a player from the database
     *
     * \param playerUuid The UUID of the player to recall
     *
     * \return The ID of the user controlling the player, or nullopt if the
     * player is not known
     */
    std::optional<Messaging::UserId> recallPlayer(const Uuid& playerUuid);

    /** \brief Retrieve deal results from the database
     *
     * \param gameUuid The UUID of the game to recall
     *
     * \return A vector containing the results of the deals in the game
     */
    std::vector<DealResult> recallDealResults(const Uuid& gameUuid);

private:

    class Impl;
    const std::unique_ptr<Impl> impl;
};

}
}

#endif // MAIN_BRIDGEGAMERECORDER_HH_
