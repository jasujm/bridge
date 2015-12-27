/** \file
 *
 * \brief Definition of an utility to extract game state from bridge engine
 */

#ifndef ENGINE_MAKEGAMESTATE_HH_
#define ENGINE_MAKEGAMESTATE_HH_

namespace Bridge {

struct GameState;

namespace Engine {

class BridgeEngine;

/** \brief Extract game state from given BridgeEngine
 *
 * \param engine the engine
 *
 * \return GameState object describing the state of the game
 */
GameState makeGameState(const BridgeEngine& engine);

}
}

#endif // ENGINE_MAKEGAMESTATE_HH_
