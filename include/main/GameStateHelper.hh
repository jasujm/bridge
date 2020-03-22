/** \file
 *
 * \brief Definition of game state helpers
 */

#ifndef MAIN_GAMESTATEHELPER_HH_
#define MAIN_GAMESTATEHELPER_HH_

#include <json.hpp>

#include <optional>

namespace Bridge {

class Player;

namespace Engine {
class BridgeEngine;
class DuplicateGameManager;
}

namespace Main {

/** \brief Helper for composing the current state of a game
 *
 * \param player The player requesting the state
 * \param engine The bridge engine
 * \param gameManager The game manager
 * \param keys The keys to retrieve
 *
 * \return Current state of the game
 */
nlohmann::json getGameState(
    const Bridge::Player& player,
    const Engine::BridgeEngine& engine,
    const Engine::DuplicateGameManager& gameManager,
    std::optional<std::vector<std::string>> keys);

}
}

#endif // MAIN_GAMESTATEHELPER_HH_
