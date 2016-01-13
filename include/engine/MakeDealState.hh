/** \file
 *
 * \brief Definition of an utility to extract deal state from bridge engine
 */

#ifndef ENGINE_MAKEGAMESTATE_HH_
#define ENGINE_MAKEGAMESTATE_HH_

namespace Bridge {

struct DealState;

namespace Engine {

class BridgeEngine;

/** \brief Extract deal state from given BridgeEngine
 *
 * \param engine the engine
 *
 * \return DealState object describing the state of the game
 */
DealState makeDealState(const BridgeEngine& engine);

}
}

#endif // ENGINE_MAKEGAMESTATE_HH_
