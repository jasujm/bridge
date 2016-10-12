/** \file
 *
 * \brief Definition of Bridge::Engine::BridgeEngine class
 */

#ifndef ENGINE_BRIDGEENINE_HH_
#define ENGINE_BRIDGEENINE_HH_

#include "bridge/Call.hh"
#include "Observer.hh"

#include <boost/core/noncopyable.hpp>
#include <boost/operators.hpp>
#include <boost/optional/optional_fwd.hpp>

#include <cstddef>
#include <memory>
#include <utility>
#include <vector>

namespace Bridge {

class Card;
class Bidding;
class Hand;
enum class Position;
class Player;
class Trick;
struct TricksWon;
struct Vulnerability;

/** \brief The bridge engine
 *
 * Namespace Engine contains BridgeEngine and the managers it directly depends
 * on.
 */
namespace Engine {

class CardManager;
class GameManager;

/** \brief The main state machine for handling a single instance of bridge game
 *
 * The responsibility of BridgeEngine is to orchestrate a game according to
 * the contract bridge rules. Some of the input and output of the engine is
 * handled by delegating to managers.
 */
class BridgeEngine : private boost::noncopyable {
public:

    /** \brief Event for announcing that a call was made
     */
    struct CallMade : private boost::equality_comparable<CallMade> {
        /** \brief Create new card played event
         *
         * \param player see \ref player
         * \param call see \ref call
         */
        CallMade(const Player& player, const Call& call);

        const Player& player; ///< \brief The player that made the call
        const Call call;      ///< \brief The call that was made
    };

    /** \brief Event for announcing that contract was reached
     */
    struct BiddingCompleted {};

    /** \brief Event for announcing that a card was played
     */
    struct CardPlayed : private boost::equality_comparable<CardPlayed> {
        /** \brief Create new card played event
         *
         * \param player see \ref player
         * \param hand see \ref hand
         * \param card see \ref card
         */
        CardPlayed(const Player& player, const Hand& hand, const Card& card);

        const Player& player; ///< \brief The player that played the card
        const Hand& hand;     ///< \brief The hand the card was played from
        const Card& card;     ///< \brief The card played
    };

    /** \brief Event for announcing that dummy has been revealed
     */
    struct DummyRevealed {};

    /** \brief Event for announcing that deal has ended
     */
    struct DealEnded {};

    /** \brief Create new bridge engine
     *
     * The engine is not started until initialize() is called. The purpose of
     * the two-stage initialization is to allow the client to subscribe to the
     * desired notifications before the engine actually starts.
     *
     * \tparam PlayerIterator an input iterator which must return a shared_ptr
     * to a player when dereferenced
     *
     * \param cardManager the card manager used to manage playing cards
     * \param gameManager the game manager used to manage the rules of the game
     * \param firstPlayer iterator to the first player in the game
     * \param lastPlayer iterator one past the last player in the game
     *
     * \throw std::invalid_argument unless there is exactly four players
     */
    template<typename PlayerIterator>
    BridgeEngine(
        std::shared_ptr<CardManager> cardManager,
        std::shared_ptr<GameManager> gameManager,
        PlayerIterator firstPlayer, PlayerIterator lastPlayer);

    ~BridgeEngine();

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

    /** \brief Subscribe to notifications about card being played
     *
     * When a card is successfully played, the notification takes place after
     * the card has been played from the hand but before the possible next
     * trick is started or (in case of the opening lead) the cards of the
     * dummy are revealed.
     */
    void subscribeToCardPlayed(std::weak_ptr<Observer<CardPlayed>> observer);

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

    /** \brief Start the game
     *
     * This method starts the actual state machine
     *
     * \note In order to not lose any notifications, notifications should be
     * subscribed to before calling this method. Especially note that after
     * starting the game, the first shuffling is immediately requested from
     * the card manager.
     */
    void initiate();

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
     *
     * \throw std::out_of_range if \p player is not in the game
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
    bool play(const Player& player, const Hand& hand, std::size_t card);

    /** \brief Determine if the game has ended
     *
     * \return true if the game has ended, false otherwise
     */
    bool hasEnded() const;

    /** \brief Retrieve the vulnerability of the current deal
     *
     * \return vulnerability for the current deal, or none if the game has
     * ended
     */
    boost::optional<Vulnerability> getVulnerability() const;

    /** \brief Retrieve the player currently in turn
     *
     * \return Pointer to the player who is next to act, or nullptr if the
     * game is not in a phase where anyone would have turn. During the playing
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
     * \return the player sitting at the position
     */
    const Player& getPlayer(Position position) const;

    /** \brief Determine the position of a given player
     *
     * \param player the player
     *
     * \return the position of the player
     *
     * \throw std::out_of_range if the player is not in the current game
     */
    Position getPosition(const Player& player) const;

    /** \brief Retrieve the hand of a given player
     *
     * \param player the player
     *
     * \return pointer the hand of the given player, or nullptr if the game is
     * not in the deal phase
     *
     * \throw std::out_of_range if the player is not in the current game
     */
    const Hand* getHand(const Player& player) const;

    /** \brief Determine whether a player is allowed to se ea hand
     *
     * If a deal is ongoing, each player sees his own hand. If the opening lead
     * has been played, each player also sees the hand of the dummy. This method
     * can be used to determine whether \p player can see \p hand according to
     * those rules.
     *
     * \param hand the hand \p player is interested in
     * \param player the player interested in \p hand
     *
     * \return true if \p player is allowed to see \p hand, false otherwise
     *
     * \throw std::out_of_range if the player is not in the current game
     */
    bool isVisible(const Hand& hand, const Player& player) const;

    /** \brief Determine the position of given hand
     *
     * \param hand the hand
     *
     * \return the position corresponding to the hand, or none if the game in
     * not in the deal phase
     *
     * \throw std::out_of_range if the hand is not in the current deal
     */
    boost::optional<Position> getPosition(const Hand& hand) const;

    /** \brief Retrieve the bidding of the current deal
     *
     * \return pointer to the bidding for the current deal, or nullptr if the
     * game is not in the deal phase
     */
    const Bidding* getBidding() const;

    /** \brief Retrieve the current trick
     *
     * \return pointer to the current trick, or nullptr if the play is not
     * ongoing
     */
    const Trick* getCurrentTrick() const;

    /** \brief Determine the number of tricks played in the current deal
     *
     * \return the number of tricks played so far, or none if the game is not
     * in the playing phase
     */
    boost::optional<std::size_t> getNumberOfTricksPlayed() const;

    /** \brief Determine the number of tricks won by each partnership
     *
     * \return TricksWon object containing tricks won by each partnership, or
     * none if the game is not in deal phase
     */
    boost::optional<TricksWon> getTricksWon() const;

    class Impl;

private:

    static std::shared_ptr<Impl> makeImpl(
        std::shared_ptr<CardManager> cardManager,
        std::shared_ptr<GameManager> gameManager,
        std::vector<std::shared_ptr<Player>> players);

    const std::shared_ptr<Impl> impl;
};

template<typename PlayerIterator>
BridgeEngine::BridgeEngine(
    std::shared_ptr<CardManager> cardManager,
    std::shared_ptr<GameManager> gameManager,
    PlayerIterator first, PlayerIterator last) :
    impl {
        makeImpl(
            std::move(cardManager),
            std::move(gameManager),
            std::vector<std::shared_ptr<Player>>(first, last))}
{
}

/** \brief Equality operator for call made events
 */
bool operator==(
    const BridgeEngine::CallMade&, const BridgeEngine::CallMade&);

/** \brief Equality operator for card played events
 */
bool operator==(
    const BridgeEngine::CardPlayed&, const BridgeEngine::CardPlayed&);

}
}

#endif // ENGINE_BRIDGEENINE_HH_
