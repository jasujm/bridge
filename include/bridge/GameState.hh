/** \file
 *
 * \brief Definition of Bridge::GameState struct
 */

#ifndef GAMESTATE_HH_
#define GAMESTATE_HH_

#include "bridge/Position.hh"
#include "bridge/Call.hh"
#include "bridge/CardType.hh"
#include "bridge/Contract.hh"
#include "bridge/DealResult.hh"
#include "bridge/Vulnerability.hh"

#include <boost/optional/optional.hpp>
#include <boost/operators.hpp>

#include <iosfwd>
#include <map>
#include <vector>
#include <utility>

namespace Bridge {

/** \brief Stage of a bridge game
 */
enum class Stage {
    SHUFFLING,
    BIDDING,
    PLAYING,
    ENDED
};

/** \brief A high level description of a bridge game
 *
 * A GameState struct is meant to be produced by game logic and consumed by
 * the UI to describe the complete state of a bridge game.
 *
 * GameState objects are equality comparable. They compare equal when every
 * aspect of two games are the same.
 *
 * \note Boost operators library is used to ensure that operator!= is
 * generated with usual semantics when operator== is supplied.
 */
struct GameState : private boost::equality_comparable<GameState> {

    /** \brief The stage of the game
     */
    Stage stage {Stage::SHUFFLING};

    /** \brief The position to act next
     *
     * This member is none if the game is not in a phase where a player can
     * act
     */
    boost::optional<Position> positionInTurn;

    /** \brief The vulnerability status of the deal
     *
     * This member if none if the game has ended
     */
    boost::optional<Vulnerability> vulnerability;

    /** \brief The known cards in the deal
     *
     * Each position is mapped to a vector of known and unplayed cards the
     * player at the position is holding. This member is none if the cards
     * haven’t been dealt yet.
     */
    boost::optional<std::map<Position, std::vector<CardType>>> cards;

    /** \brief The calls made in the auction of the current deal
     *
     * This member is none if the bidding hasn’t started yet. If the bidding
     * has started, it is a vector of pairs (in calling order) containing the
     * position of the caller and the call made.
     */
    boost::optional<std::vector<std::pair<Position, Call>>> calls;

    /** \brief Result of the bidding round
     */
    struct BiddingResult {
        Position declarer; ///< \brief The position of the declarer
        Contract contract; ///< \brief The contract made by the declarer
    };

    /** \brief The result of the bidding round
     *
     * This member is none if the bidding isn’t finished
     */
    boost::optional<BiddingResult> biddingResult;

    /** \brief Result of the playing phase
     */
    struct PlayingResult {
        /** \brief Cards played to the current trick
         *
         * This member is a mapping from positions that have already played to
         * the trick, to the cards they have played.
         */
        std::map<Position, CardType> currentTrick;
        DealResult dealResult; ///< \brief The result of the current deal
    };

    /** \brief The result of the playing phase
     *
     * This member is none if the playing phase hasn’t started yet
     */
    boost::optional<PlayingResult> playingResult;
};

/** \brief Equality operator for game states
 *
 * \sa GameState
 */
bool operator==(const GameState&, const GameState&);

/** \brief Output a Stage to stream
 *
 * \param os the output stream
 * \param stage the stage to output
 *
 * \return parameter \p os
 */
std::ostream& operator<<(std::ostream& os, Stage stage);

/** \brief Output a GameState to stream
 *
 * \param os the output stream
 * \param state the game state to output
 *
 * \return parameter \p os
 */
std::ostream& operator<<(std::ostream& os, const GameState& state);

}

#endif // GAMESTATE_HH_
