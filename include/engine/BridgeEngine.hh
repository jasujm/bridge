/** \file
 *
 * \brief Definition of Bridge::Engine::BridgeEngine class
 */

#ifndef ENGINE_BRIDGEENINE_HH_
#define ENGINE_BRIDGEENINE_HH_

#include "bridge/Call.hh"
#include "Observer.hh"

#include <boost/core/noncopyable.hpp>
#include <boost/optional/optional_fwd.hpp>

#include <algorithm>
#include <cstddef>
#include <memory>
#include <utility>
#include <vector>

namespace Bridge {

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
 *
 * \todo Add reporting failures (plays out of turns, calls that are against
 * rules etc.) using error codes or some other mechanism.
 */
class BridgeEngine : private boost::noncopyable {
public:

    /** \brief Tag for announcing that deal has ended
     */
    struct DealEnded {};

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

    /** \brief Subscribe to notifications about deal ending
     *
     * BridgeEngine notifies its observers when a deal has ended.
     */
    void subscribe(std::weak_ptr<Observer<DealEnded>> observer);

    /** \brief Make call
     *
     * Announce that \p player wants to make \p call. This method does nothing
     * if the call fails for any reason. If the method throws an exception,
     * the state machine is in unspecified state.
     *
     * \param player the player who wants to make the call
     * \param call the call to be made
     */
    void call(const Player& player, const Call& call);

    /** \brief Play card
     *
     * Announce that \p player wants to play \p card. This method does nothing
     * if the call fails for any reason. If the method throws an exception,
     * the state machine is in unspecified state.
     *
     * \param player the player who wants to make the call
     * \param card the index referring to a card in the hand of \p player
     */
    void play(const Player& player, std::size_t card);

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

    /** \brief Retrieve the player currently in turn§
     *
     * \return pointer to the player who is next to act, or nullptr if the
     * game is not in a phase where anyone would have turn
     */
    const Player* getPlayerInTurn() const;

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

    /** \brief Retrieve all hands visible to the given player
     *
     * If a deal is ongoing, each player sees his own hand. If the opening
     * lead has been played, each player also sees the hand of the dummy. At
     * most two hands will be output.
     *
     * \tparam OutputIterator an output iterator which must accept const Hand&
     *
     * \param player the player
     * \param out the iterator to which the the visible hands will be written
     *
     * \return one past the position the last hand was written to
     *
     * \throw std::out_of_range if the player is not in the current game
     */
    template<typename OutputIterator>
    OutputIterator getVisibleHands(
        const Player& player, OutputIterator out) const;

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

    const Hand* getDummyHandIfVisible() const;

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

template<typename OutputIterator>
OutputIterator BridgeEngine::getVisibleHands(
    const Player& player, OutputIterator out) const
{
    std::array<const Hand*, 2> hands {
        {getHand(player), getDummyHandIfVisible()}};
    const auto first = hands.begin();
    auto last = hands.end();
    last = std::remove(first, last, nullptr);
    last = std::unique(first, last);
    return std::transform(
        first, last, out, [](const auto* hand) -> decltype(auto)
        {
            assert(hand);
            return *hand;
        });
}

}
}

#endif // ENGINE_BRIDGEENINE_HH_
