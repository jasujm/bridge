/** \file
 *
 * \brief Definition of game state helpers
 */

#ifndef MAIN_GAMESTATEHELPER_HH_
#define MAIN_GAMESTATEHELPER_HH_

#include <nlohmann/json.hpp>

#include <optional>

#include "bridge/CardType.hh"
#include "bridge/Position.hh"

namespace Bridge {

class Deal;
class Hand;

namespace Main {

/** \brief Representation of the state of the game
 */
using GameState = nlohmann::json;

/** \brief Helper for extracting cards from a hand
 *
 * \param hand A hand object
 *
 * \return A vector containing CardType object for each visible card
 * in the hand, and none for each non‐visible card
 */
std::vector<std::optional<CardType>> getCardsFromHand(const Hand& hand);

/** \brief Emplace the pubstate subobject of a deal to \p out
 *
 * This implements creating the pubstate subobject in the \ref
 * bridgeprotocolcontrolget command
 *
 * \param deal the deal
 * \param out a JSON object where the pubstate subobject is emplaced
 */
void emplacePubstate(const Deal* deal, GameState& out);

/** \brief Emplace the privstate subobject of a deal to \p out
 *
 * This implements creating the privstate subobject in the \ref
 * bridgeprotocolcontrolget command
 *
 * \param deal the deal
 * \param playerPosition the position of the player whose viewpoint is applied
 * \param out a JSON object where the priv subobject is emplaced
 */
void emplacePrivstate(
    const Deal* deal, const std::optional<Position>& playerPosition,
    GameState& out);

/** \brief Emplace the self subobject of a deal to \p out
 *
 * This implements creating the privstate subobject in the \ref
 * bridgeprotocolcontrolget command
 *
 * \param deal the deal
 * \param playerPosition the position of the player whose viewpoint is applied
 * \param out a JSON object where the priv subobject is emplaced
 */
void emplaceSelf(
    const Deal* deal, const std::optional<Position>& playerPosition,
    GameState& out);

}
}

#endif // MAIN_GAMESTATEHELPER_HH_
