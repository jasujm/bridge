/** \file
 *
 * \brief Definition of Bridge::Engine::BridgeEngine class
 */

#ifndef ENGINE_BRIDGEENINE_HH_
#define ENGINE_BRIDGEENINE_HH_

#include "bridge/Call.hh"
#include "bridge/Position.hh"
#include "bridge/Uuid.hh"
#include "bridge/Vulnerability.hh"
#include "engine/GameManager.hh"
#include "Observer.hh"

#include <any>
#include <stdexcept>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

namespace Bridge {

class Card;
class Bidding;
struct Contract;
class Deal;
class Hand;
class Player;
class Trick;

/** \brief The bridge engine
 *
 * Namespace Engine contains BridgeEngine and the managers it directly depends
 * on.
 */
namespace Engine {

class CardManager;

/** \brief Exception to indicate unexpected events in a bridge game
 */
struct BridgeEngineFailure : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

/** \brief The main state machine for handling a single instance of bridge game
 *
 * The responsibility of BridgeEngine is to orchestrate a game according to
 * the contract bridge rules. Some of the input and output of the engine is
 * handled by delegating to managers.
 *
 * Players can be added to the game and replaced with other players at any
 * point. A game can proceed even if all players are not seated, although only
 * player seated to a position that has turn can make calls and play cards.
 */
class BridgeEngine {
public:

    /** \brief Event for announcing that deal has started
     */
    struct DealStarted {
        /** \brief Create new deal started event
         *
         * \param uuid see \ref uuid
         * \param opener see \ref opener
         * \param vulnerability see \ref vulnerability
         */
        DealStarted(
            const Uuid& uuid, Position opener, Vulnerability vulnerability);

        const Uuid& uuid;            ///< UUID of the deal
        Position opener;             ///< The position of the opened
        Vulnerability vulnerability; ///< Vulnerabilities of the deal
    };

    /** \brief Event for announcing that a player has turn
     */
    struct TurnStarted {
        /** \brief Create new turn started event
         *
         * \param uuid see \ref uuid
         * \param position see \ref position
         */
        TurnStarted(const Uuid& uuid, Position position);

        const Uuid& uuid;            ///< UUID of the deal
        Position position;  ///< The position of the player having turn
    };

    /** \brief Event for announcing that a call was made
     */
    struct CallMade {
        /** \brief Create new card played event
         *
         * \param uuid see \ref uuid
         * \param position see \ref position
         * \param call see \ref call
         * \param index see \ref index
         */
        CallMade(const Uuid& uuid, Position position, const Call& call, int index);

        const Uuid& uuid;   ///< UUID of the deal
        Position position;  ///< \brief The position that made the call
        Call call;          ///< \brief The call that was made
        int index;          ///< \brief The index of the call in the bidding
    };

    /** \brief Event for announcing that contract was reached
     */
    struct BiddingCompleted {
        /** \brief Generate new bidding completed event
         *
         * \param uuid see \ref uuid
         * \param declarer see \ref declarer
         * \param contract see \ref contract
         */
        BiddingCompleted(const Uuid& uuid, Position declarer, const Contract& contract);

        const Uuid& uuid;         ///< UUID of the deal
        Position declarer;        ///< \brief The declarer determined by the bidding
        const Contract& contract; ///< \brief The contract reached during bidding
    };

    /** \brief Event for announcing that a trick has started
     */
    struct TrickStarted {
        /** \brief Create new trick completed event
         *
         * \param uuid see \ref uuid
         * \param leader see \ref leader
         */
        TrickStarted(const Uuid& uuid, Position leader);

        const Uuid& uuid;    ///< \brief UUID of the deal
        Position leader;     ///< \brief The leader position
    };

    /** \brief Event for announcing that a card was played
     */
    struct CardPlayed {
        /** \brief Create new card played event
         *
         * \param uuid see \ref uuid
         * \param position see \ref position
         * \param card see \ref card
         * \param trickIndex see \ref trickIndex
         * \param index see \ref index
         */
        CardPlayed(
            const Uuid& uuid, Position position, const Card& card,
            int trickIndex, int index);

        const Uuid& uuid;   ///< UUID of the deal
        Position position;  ///< \brief The position the card was played from
        const Card& card;   ///< \brief The card played
        int trickIndex;     ///< \brief The zero based index of the trick in the deal
        int index;          ///< \brief The zero based index of the card in the trick
    };

    /** \brief Event for announcing that a trick was completed
     */
    struct TrickCompleted {
        /** \brief Create new trick completed event
         *
         * \param uuid see \ref uuid
         * \param trick see \ref trick
         * \param winner see \ref winner
         * \param index see \ref index
         */
        TrickCompleted(const Uuid& uuid, const Trick& trick, Position winner, int index);

        const Uuid& uuid;    ///< UUID of the deal
        const Trick& trick;  ///< \brief The trick that was completed
        Position winner;     ///< \brief The winner position
        int index;           ///< \brief The zero based index of the trick in the deal
    };

    /** \brief Event for announcing that dummy has been revealed
     */
    struct DummyRevealed {
        /** ]brief Create new dummy revealed event
         *
         * \param uuid see \ref uuid
         * \param position see \ref position
         * \param hand see \ref hand
         */
        DummyRevealed(const Uuid& uuid, Position position, const Hand& hand);

        const Uuid& uuid;        ///< UUID of the deal
        Position position;       ///< \brief The position of the dummy
        const Hand& hand;        ///< \brief The hand of the dummy
    };

    /** \brief Event for announcing that deal has ended
     */
    struct DealEnded {
        /** \brief Create new deal ended event
         *
         * \param uuid see \ref uuid
         * \param contract see \ref contract
         * \param tricksWon see \ref tricksWon
         * \param result see \ref result
         */
        DealEnded(
            const Uuid& uuid,
            const Contract* contract,
            std::optional<int> tricksWon,
            const GameManager::ResultType& result);

        const Uuid& uuid;  ///< UUID of the deal

        /// \brief Contract declared, or nullptr if the deal passed out
        const Contract* contract;

        /// \brief Tricks won by the declarer, or nullopt if the deal passed out
        std::optional<int> tricksWon;

        /** \brief Result of the deal
         *
         * \note This is the object returned by the game manager. The client
         * needs to interpret it according to the type of the game manager.
         */
        const GameManager::ResultType& result;
    };

    /** \brief Create new bridge engine
     *
     * The first deal is not started until startDeal() is called. The two-stage
     * initialization allows the client to subscribe to the desired
     * notifications before the game actually starts.
     *
     * If the \p deal parameter is given, the bidding and playing phases of the
     * deal are replayed on the first call of startDeal(). No notifications
     * about the recalled deal are published. If the deal state doesn’t
     * represent a valid bridge deal, an expection is thrown and the engine is
     * left in an unspecified state.
     *
     * \param cardManager the card manager used to manage playing cards
     * \param gameManager the game manager used to manage the rules of the game
     * \param deal The deal to recall, or nullptr if a new deal should
     * be started
     */
    BridgeEngine(
        std::shared_ptr<CardManager> cardManager,
        std::shared_ptr<GameManager> gameManager,
        std::unique_ptr<const Deal> deal = nullptr);

    /** \brief Move constructor
     */
    BridgeEngine(BridgeEngine&&) = default;

    ~BridgeEngine();

    /** \brief Move assignment
     */
    BridgeEngine& operator=(BridgeEngine&&) = default;

    /** \brief Subscribe to notifications about deal being started
     *
     * The notification takes place after hands have become visible but before
     * bidding has started.
     */
    void subscribeToDealStarted(
        std::weak_ptr<Observer<DealStarted>> observer);

    /** \brief Subscribe to notifications about turn starting
     *
     * The notification takes place after a player has got turn to call or play
     * card.
     */
    void subscribeToTurnStarted(
        std::weak_ptr<Observer<TurnStarted>> observer);

    /** \brief Subscribe to notifications about call being made
     *
     * When a call is successfully made, the notification takes place after
     * the call has been added to the bidding but before the playing phase (in
     * case of completed bidding) or next deal (in case of passed out biddin)
     * starts.
     */
    void subscribeToCallMade(std::weak_ptr<Observer<CallMade>> observer);

    /** \brief Subscribe to notifications about bidding completed
     *
     * The notification takes place when contract has been reached. If the
     * bidding was passed out, this notification does not take place.
     */
    void subscribeToBiddingCompleted(
        std::weak_ptr<Observer<BiddingCompleted>> observer);

    /** \brief Subscribe to notifications about trick being started
     *
     * This notification takes place when a trick has started.
     */
    void subscribeToTrickStarted(
        std::weak_ptr<Observer<TrickStarted>> observer);

    /** \brief Subscribe to notifications about card being played
     *
     * When a card is successfully played, the notification takes place after
     * the card has been played from the hand but before the possible trick is
     * completed or (in case of the opening lead) the cards of the dummy are
     * revealed.
     */
    void subscribeToCardPlayed(std::weak_ptr<Observer<CardPlayed>> observer);

    /** \brief Subscribe to notifications about trick being completed
     *
     * This notification takes place when trick has been completed and awarded
     * to the winner, but before the next trick is started.
     */
    void subscribeToTrickCompleted(
        std::weak_ptr<Observer<TrickCompleted>> observer);

    /** \brief Subscribe to notifications about dummy being revealed
     *
     * When the opening lead is successfully played and dummy hand has been
     * revealed, this notification takes place. In the notification handler
     * and after that the dummy cards are visible.
     */
    void subscribeToDummyRevealed(
        std::weak_ptr<Observer<DummyRevealed>> observer);

    /** \brief Subscribe to notifications about deal ending
     *
     * When a deal is ended, the notification takes place after the results of
     * the old deal are visible but before shuffling the cards for the next
     * deal has started.
     */
    void subscribeToDealEnded(std::weak_ptr<Observer<DealEnded>> observer);

    /** \brief Start a new deal
     *
     * This method starts a deal if no deal is ongoing. It needs to be called
     * before the game and after the completion of each deal when the client is
     * ready to start a deal.
     *
     * \note In order to not lose any otifications, notifications should be
     * subscribed to before calling this method for the first time. Especially
     * note that after starting the game, the first shuffling is immediately
     * requested from the card manager.
     *
     * \throw BridgeEngineFailure if an error occurs when recalling the deal
     * given as constructor argument
     */
    void startDeal();

    /** \brief Add player to the game
     *
     * Make \p player seated in given \p position. If a player already exists in
     * the position, he is replaced with the new player.
     *
     * The method will have no effect if \p player is already in the game.
     *
     * \param position the position to seat the \p player in
     * \param player the player
     *
     * \return true if \p player was successfully added to \p
     * position, false otherwise
     */
    bool setPlayer(Position position, std::shared_ptr<Player> player);

    /** \brief Make call
     *
     * Make \p call by \p player. The method call succeeds if \p player has
     * turn and \p call is allowed to be made by the player. Otherwise the
     * call does nothing.
     *
     * \warning This function is not reentrant and may not be called from any
     * of the observers.
     *
     * \param player the player who wants to make the call
     * \param call the call to be made
     *
     * \return true if the call is successful, false otherwise
     */
    bool call(const Player& player, const Call& call);

    /** \brief Play card
     *
     * Play \p card from \p hand controlled by \p player. The method call
     * succeeds if the player has turn, plays the card from the correct hand
     * and the card can be played to the current trick. The declarer plays
     * from both his and dummy’s hand, i.e. has two turns per trick. Otherwise
     * the call does nothing.
     *
     * \warning This function is not reentrant and may not be called from any
     * of the observers.
     *
     * \param player the player who wants to play the card
     * \param hand the hand from which the card is played
     * \param card the index referring to a card in the \p hand
     *
     * \return true if the play is successful, false otherwise
     *
     * \throw std::out_of_range if \p player or \p hand is not in the game
     */
    bool play(const Player& player, const Hand& hand, int card);

    /** \brief Determine if the game has ended
     *
     * \return true if the game has ended, false otherwise
     */
    bool hasEnded() const;

    /** \brief Get the record of the current deal
     *
     * \return Deal object representing the current deal, or nullptr
     * if no deal is ongoing.
     */
    const Deal* getCurrentDeal() const;

    /** \brief Retrieve the player currently in turn
     *
     * \return Pointer to the player who is next to act. The method returns
     * nullptr if the game is not in a phase where anyone would have turn, or no
     * player is seated at the position that would have turn. During the playing
     * phase declarer takes turns instead of dummy.
     */
    const Player* getPlayerInTurn() const;

    /** \brief Retrieve the hand that plays to the trick next
     *
     * \return pointer to the hand from which the next card is played to the
     * trick, or nullptr if the game is not in the playing phase
     */
    const Hand* getHandInTurn() const;

    /** \brief Determine the player at given position
     *
     * \param position the position
     *
     * \return the player seated at the position, or nullptr if there is no
     * player in the position
     */
    const Player* getPlayer(Position position) const;

    /** \brief Determine the position of a given player
     *
     * \param player the player
     *
     * \return the position of the player, or none if the player is not in the
     * game
     */
    std::optional<Position> getPosition(const Player& player) const;

    class Impl;

private:

    std::shared_ptr<Impl> impl;
};

/** \brief Equality operator for deal started events
 */
bool operator==(
    const BridgeEngine::DealStarted&, const BridgeEngine::DealStarted&);

/** \brief Equality operator for turn started events
 */
bool operator==(
    const BridgeEngine::TurnStarted&, const BridgeEngine::TurnStarted&);

/** \brief Equality operator for call made events
 */
bool operator==(
    const BridgeEngine::CallMade&, const BridgeEngine::CallMade&);

/** \brief Equality operator for bidding completed events
 */
bool operator==(
    const BridgeEngine::BiddingCompleted&,
    const BridgeEngine::BiddingCompleted&);

/** \brief Equality operator for trick started events
 */
bool operator==(
    const BridgeEngine::TrickStarted&, const BridgeEngine::TrickStarted&);

/** \brief Equality operator for card played events
 */
bool operator==(
    const BridgeEngine::CardPlayed&, const BridgeEngine::CardPlayed&);

/** \brief Equality operator for trick completed events
 */
bool operator==(
    const BridgeEngine::TrickCompleted&, const BridgeEngine::TrickCompleted&);

/** \brief Equality operator for dummy revealed events
 */
bool operator==(
    const BridgeEngine::DummyRevealed&, const BridgeEngine::DummyRevealed&);

}
}

#endif // ENGINE_BRIDGEENINE_HH_
