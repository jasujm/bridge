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
#include "bridge/Vulnerability.hh"

#include <iosfwd>
#include <map>
#include <optional>
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

/** \brief A high level description of a bridge game
 *
 * A DealState struct is meant to be produced by game logic and consumed by
 * the UI to describe the complete state of a bridge deal.
 *
 * DealState objects are equality comparable. They compare equal when every
 * aspect of two deals
 are the same.
 *
 * \deprecated This class was used to aid serializing the state of a single deal
 * into message sent to the clients. Currently only the unit test for
 * Engine::BridgeEngine depends on this class, and it is no longer part of the
 * actual bridge application.
 */
struct DealState {

    /** \brief The stage of the game
     */
    Stage stage {Stage::SHUFFLING};

    /** \brief The position to act next
     *
     * This member is none if the game is not in a phase where a player can
     * act.
     */
    std::optional<Position> positionInTurn;

    /** \brief The vulnerability status of the deal
     *
     * This member if none if the game has ended.
     */
    std::optional<Vulnerability> vulnerability;

    /** \brief Type of the \ref cards member
     */
    using Cards = std::map<Position, std::vector<CardType>>;

    /** \brief The known cards in the deal
     *
     * Each position is mapped to a vector of known and unplayed cards the
     * player at the position is holding. This member is none if the cards
     * haven’t been dealt yet.
     */
    std::optional<Cards> cards;

    /** \brief Type of the \ref calls member
     */
    using Calls = std::vector<std::pair<Position, Call>>;

    /** \brief The calls made in the auction of the current deal
     *
     * This member is none if the bidding hasn’t started yet. If the bidding
     * has started, it is a vector of pairs (in calling order) containing the
     * position of the caller and the call made.
     */
    std::optional<Calls> calls;

    /** \brief The declarer determined by the bidding
     *
     * This member is none if the bidding isn’t finished.
     */
    std::optional<Position> declarer;

    /** \brief The contract made by the declarer
     *
     * This member is none if the bidding isn’t finished.
     */
    std::optional<Contract> contract;

    /** \brief Type of the \ref currentTrick member
     */
    using Trick = std::vector<std::pair<Position, CardType>>;

    /** \brief Cards played to the current trick
     *
     * If the playing has started, this member is a vector of pairs containing
     * position of the player and the card played, in the order the cards were
     * played.
     */
    std::optional<Trick> currentTrick;

    /** \brief Equality comparison
     */
    bool operator==(const DealState&) const = default;
};

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
