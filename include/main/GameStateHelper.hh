/** \file
 *
 * \brief Definition of game state helpers
 */

#ifndef MAIN_GAMESTATEHELPER_HH_
#define MAIN_GAMESTATEHELPER_HH_

#include <nlohmann/json.hpp>

#include <optional>

#include "bridge/CardType.hh"
#include "bridge/Uuid.hh"

namespace Bridge {

class Hand;
class Player;

namespace Engine {
class BridgeEngine;
class DuplicateGameManager;
}

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
 * \param player The player requesting the state
 * \param engine The bridge engine
 * \param gameManager The game manager
 * \param dealUuid The UUID of the current deal
 * \param keys The keys to retrieve
 *
 * \return Current state of the game
 */
nlohmann::json getGameState(
    const Player& player,
    const Engine::BridgeEngine& engine,
    const Engine::DuplicateGameManager& gameManager,
    const Uuid& dealUuid,
    std::optional<std::vector<std::string>> keys);

}
}

#endif // MAIN_GAMESTATEHELPER_HH_
