/** \file
 *
 * \brief Definition of Bridge::Engine::BridgeEngine class
 */

#ifndef ENGINE_BRIDGEENINE_HH_
#define ENGINE_BRIDGEENINE_HH_

#include "bridge/Call.hh"

#include <boost/bimap/bimap.hpp>
#include <boost/optional/optional_fwd.hpp>
#include <boost/statechart/custom_reaction.hpp>
#include <boost/statechart/event.hpp>
#include <boost/statechart/state.hpp>
#include <boost/statechart/state_machine.hpp>
#include <boost/statechart/termination.hpp>

#include <cstddef>
#include <functional>
#include <memory>
#include <utility>
#include <vector>

namespace Bridge {

class Bidding;
struct DealResult;
class Hand;
enum class Position;
class Player;
class Trick;
enum class Vulnerability;

/** \brief The bridge engine
 *
 * Namespace Engine contains BridgeEngine and the managers it directly depends
 * on.
 */
namespace Engine {

class CardManager;
class GameManager;

////////////////////////////////////////////////////////////////////////////////
// Events
////////////////////////////////////////////////////////////////////////////////

/** \brief Event for signaling that the shuffling is completed
 */
class ShuffledEvent : public boost::statechart::event<ShuffledEvent> {};

/** \brief Event for signaling a call made by a player
 */
class CallEvent : public boost::statechart::event<CallEvent> {
public:

    /** \brief the player who makes the call
     */
    std::reference_wrapper<const Player> player;

    /** \brief call the call to be made
     */
    Call call;

    /** \brief Create call event
     *
     * \param player the player who makes the call
     * \param call the call to be made
     */
    CallEvent(const Player& player, const Call& call) :
        player {player},
        call {call}
    {
    }
};

/** \brief Event for signaling a play card made by a player
 */
class PlayCardEvent : public boost::statechart::event<PlayCardEvent> {
public:

    /** \brief the player who plays the card
     */
    std::reference_wrapper<Player> player;

    /** \brief the index of the card to be played
     */
    std::size_t card;

    /** \brief Create play card event
     *
     * \param player the player who plays the card
     * \param card the index of the card to be played
     */
    PlayCardEvent(Player& player, std::size_t card) :
        player {player},
        card {card}
    {
    }
};

////////////////////////////////////////////////////////////////////////////////
// State machine
////////////////////////////////////////////////////////////////////////////////

class Shuffling;
class InDeal;

/** \brief The main state machine for handling a single instance of bridge game
 *
 * The responsibility of BridgeEngine is to orchestrate a game according to
 * the contract bridge rules. Some of the input and output of the engine is
 * handled by delegating to managers.
 *
 * The BridgeEngine uses Boost Statechart library. See
 * http://www.boost.org/doc/libs/1_60_0/libs/statechart/doc/index.html for the
 * documentation of Statechart.
 */
class BridgeEngine :
        public boost::statechart::state_machine<BridgeEngine, Shuffling> {
public:

    /** \brief Create new bridge engine
     *
     * \tparam PlayerIterator an input iterator which must return a shared_ptr
     * to a player when dereferenced
     *
     * \param cardManager the card manager used to manage playing cards
     * \param gameManager the game manager used to manage the rules of the game
     * \param firstPlayer the first player in the game
     * \param lastPlayer the last player in the game
     *
     * \throw std::invalid_argument unless there is exactly four players
     */
    template<typename PlayerIterator>
    BridgeEngine(
        std::shared_ptr<CardManager> cardManager,
        std::shared_ptr<GameManager> gameManager,
        PlayerIterator firstPlayer, PlayerIterator lastPlayer);

    ~BridgeEngine();

    /** \brief Retrieve the vulnerability of the current deal
     *
     * \return vulnerability for the current deal, or none if the game has
     * ended
     */
    boost::optional<Vulnerability> getVulnerability() const;

    /** \brief Retrieve the player currently in turn§
     *
     * \return the player who is next to act, or none if the game is not in a
     * phase where anyone would have turn
     */
    boost::optional<const Player&> getPlayerInTurn() const;

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
     * \return the hand of the given player, or none if the game is not in the
     * deal phase
     *
     * \throw std::out_of_range if the player is not in the current game
     */
    boost::optional<const Hand&> getHand(const Player& player) const;

    /** \brief Retrieve the bidding of the current deal
     *
     * \return bidding for the current deal, or none if the game is not in the
     * deal phase
     */
    boost::optional<const Bidding&> getBidding() const;

    /** \brief Retrieve the current trick
     *
     * \return the current trick, or none if the play is not ongoing
     */
    boost::optional<const Trick&> getCurrentTrick() const;

    /** \brief Determine the number of tricks played in the current deal
     *
     * \return the number of tricks played so far, or none if the game is not
     * in the playing phase
     */
    boost::optional<std::size_t> getNumberOfTricksPlayed() const;

    /** \brief Determine the deal result
     *
     * \return the deal result of the current deal, or none if game is not in
     * playing phase
     */
    boost::optional<DealResult> getDealResult() const;

private:

    void init();

    std::shared_ptr<CardManager> cardManager;
    std::shared_ptr<GameManager> gameManager;
    std::vector<std::shared_ptr<Player>> players;
    boost::bimaps::bimap<Position, Player*> playersMap;

    friend class Shuffling;
    friend class InDeal;
};

template<typename PlayerIterator>
inline BridgeEngine::BridgeEngine(
    std::shared_ptr<CardManager> cardManager,
    std::shared_ptr<GameManager> gameManager,
    PlayerIterator first, PlayerIterator last) :
    cardManager {std::move(cardManager)},
    gameManager {std::move(gameManager)},
    players(first, last)
{
    init();
}

/// \cond BRIDGE_ENGINE_INTERNALS

class GameEndedEvent : public boost::statechart::event<GameEndedEvent> {};

// Definition of the starting state must be visible when instantiating
// BridgeEngine
class Shuffling : public boost::statechart::state<Shuffling, BridgeEngine> {
public:
    using reactions = boost::mpl::list<
        boost::statechart::custom_reaction<ShuffledEvent>,
        boost::statechart::termination<GameEndedEvent>>;

    Shuffling(my_context ctx);

    boost::statechart::result react(const ShuffledEvent&);
};

/// \endcond

}
}

#endif // ENGINE_BRIDGEENINE_HH_
