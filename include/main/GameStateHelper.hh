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

/** \brief Helper for extracting cards from a hand
 *
 * \param hand A hand object
 *
 * \return A vector containing CardType object for each visible card
 * in the hand, and none for each non‐visible card
 */
std::vector<std::optional<CardType>> getCardsFromHand(const Hand& hand);

/** \brief Helper for composing the current state of a game
 *
 * \param deal The current deal
 * \param playerPosition The position of the player in the game
 * \param playerHasTurn Flag indicating if the player has turn
 * \param keys The keys to retrieve
 *
 * \return Current state of the game
 */
nlohmann::json getGameState(
    const Deal* deal,
    std::optional<Position> playerPosition,
    bool playerHasTurn,
    std::optional<std::vector<std::string>> keys);

}
}

#endif // MAIN_GAMESTATEHELPER_HH_
