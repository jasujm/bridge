/** \file
 *
 * \brief Definition of Bridge::DealState struct
 */

#ifndef DEALSTATE_HH_
#define DEALSTATE_HH_

#include "bridge/Position.hh"
#include "bridge/Call.hh"
#include "bridge/CardType.hh"
#include "bridge/Contract.hh"
#include "bridge/TricksWon.hh"
#include "bridge/Vulnerability.hh"

#include <boost/bimap/bimap.hpp>
#include <boost/optional/optional.hpp>
#include <boost/operators.hpp>

#include <iosfwd>
#include <map>
#include <vector>
#include <string>
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

/** \brief Type of \ref STAGE_TO_STRING_MAP
 */
using StageToStringMap = boost::bimaps::bimap<Stage, std::string>;

/** \brief Two-way map between Stage enumerations and their string
 * representation
 */
extern const StageToStringMap STAGE_TO_STRING_MAP;

/** \brief A high level description of a bridge game
 *
 * A DealState struct is meant to be produced by game logic and consumed by
 * the UI to describe the complete state of a bridge deal.
 *
 * DealState objects are equality comparable. They compare equal when every
 * aspect of two deals
 are the same.
 *
 * \note Boost operators library is used to ensure that operator!= is
 * generated with usual semantics when operator== is supplied.
 */
struct DealState : private boost::equality_comparable<DealState> {

    /** \brief The stage of the game
     */
    Stage stage {Stage::SHUFFLING};

    /** \brief The position to act next
     *
     * This member is none if the game is not in a phase where a player can
     * act.
     */
    boost::optional<Position> positionInTurn;

    /** \brief Type of the \ref allowedCalls member
     */
    using AllowedCalls = std::vector<Call>;

    /** \brief The calls the current player is allowed to make in the bidding
     *
     * This member is none if the deal is not in the bidding stage.
     */
    boost::optional<AllowedCalls> allowedCalls;

    /** \brief Type of the \ref allowedCards member
     */
    using AllowedCards = std::vector<CardType>;

    /** \brief The cards the current player is allowed to play to the current
     * trick
     *
     * This member is none if the deal is not in the playing stage.
     */
    boost::optional<AllowedCards> allowedCards;

    /** \brief The vulnerability status of the deal
     *
     * This member if none if the game has ended.
     */
    boost::optional<Vulnerability> vulnerability;

    /** \brief Type of the \ref cards member
     */
    using Cards = std::map<Position, std::vector<CardType>>;

    /** \brief The known cards in the deal
     *
     * Each position is mapped to a vector of known and unplayed cards the
     * player at the position is holding. This member is none if the cards
     * haven’t been dealt yet.
     */
    boost::optional<Cards> cards;

    /** \brief Type of the \ref calls member
     */
    using Calls = std::vector<std::pair<Position, Call>>;

    /** \brief The calls made in the auction of the current deal
     *
     * This member is none if the bidding hasn’t started yet. If the bidding
     * has started, it is a vector of pairs (in calling order) containing the
     * position of the caller and the call made.
     */
    boost::optional<Calls> calls;

    /** \brief The declarer determined by the bidding
     *
     * This member is none if the bidding isn’t finished.
     */
    boost::optional<Position> declarer;

    /** \brief The contract made by the declarer
     *
     * This member is none if the bidding isn’t finished.
     */
    boost::optional<Contract> contract;

    /** \brief Type of the \ref currentTrick member
     */
    using Trick = std::vector<std::pair<Position, CardType>>;

    /** \brief Cards played to the current trick
     *
     * If the playing has started, this member is a vector of pairs containing
     * position of the player and the card played, in the order the cards were
     * played.
     */
    boost::optional<Trick> currentTrick;

    /** \brief The number of tricks won in the current deal
     *
     * This member if none if the playing hasn’t started yet.
     */
    boost::optional<TricksWon> tricksWon;
};

/** \brief Equality operator for deal states
 *
 * \sa DealState
 */
bool operator==(const DealState&, const DealState&);

/** \brief Output a Stage to stream
 *
 * \param os the output stream
 * \param stage the stage to output
 *
 * \return parameter \p os
 */
std::ostream& operator<<(std::ostream& os, Stage stage);

/** \brief Output a DealState to stream
 *
 * \param os the output stream
 * \param state the deal state to output
 *
 * \return parameter \p os
 */
std::ostream& operator<<(std::ostream& os, const DealState& state);

}

#endif // DEALSTATE_HH_
