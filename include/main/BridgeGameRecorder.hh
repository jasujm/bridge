/** \file
 *
 * \brief Definition of Bridge::Main::BridgeGameRecorder
 */

#ifndef MAIN_BRIDGEGAMERECORDER_HH_
#define MAIN_BRIDGEGAMERECORDER_HH_

#include "bridge/Uuid.hh"
#include "messaging/Identity.hh"

#include <memory>
#include <optional>
#include <string_view>

namespace Bridge {

namespace Engine {
class GameManager;
class CardManager;
}

class Deal;

namespace Main {

/** \brief Persistence for bridge games
 *
 * A BridgeGameRecorder listens to events from bridge games and records the
 * deals to a database. It allows recalling a deal from the recorded data.
 *
 * The recorder is only compiled if WITH_RECORDER macro is defined with true
 * value. Otherwise a stub implementing this interface is compiled.
 *
 * \todo This class is experimental and the format it uses to save the game is
 * not well specified and may change. So it is not suitable for long term
 * storing.
 *
 * \todo Use rocksdb merge operations to record individual calls, cards,
 * etc. smarter
 */
class BridgeGameRecorder {
public:

    /** \brief Objects for creating new deal state
     */
    struct DealState {
        std::unique_ptr<Deal> deal;
        std::shared_ptr<Engine::CardManager> cardManager;
        std::shared_ptr<Engine::GameManager> gameManager;
    };

    /** \brief Objects for creating new game state
     */
    struct GameState {
        std::optional<Uuid> dealUuid;
        std::optional<Uuid> playerUuid[4];
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

    /** \brief Record information \p player
     *
     * \param playerUuid The UUID of the player to record
     * \param userId The ID of the user controlling the player
     */
    void recordPlayer(const Uuid& playerUuid, Messaging::UserIdView userId);

    /** \brief Retrieve a game from the database
     *
     * \param gameUuid The UUID of the game to recall
     *
     * \return UUID of the current deal, or nullopt if the game is not known
     */
    std::optional<GameState> recallGame(const Uuid& gameUuid);

    /** \brief Retrieve a deal from the database
     *
     * \param gameUuid The UUID of the deal to recall
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

private:

    class Impl;
    const std::unique_ptr<Impl> impl;
};

}
}

#endif // MAIN_BRIDGEGAMERECORDER_HH_
