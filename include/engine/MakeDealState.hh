/** \file
 *
 * \brief Definition of an utility to extract deal state from bridge engine
 */

#ifndef ENGINE_MAKEGAMESTATE_HH_
#define ENGINE_MAKEGAMESTATE_HH_

namespace Bridge {

struct DealState;
class Player;

namespace Engine {

class BridgeEngine;

/** \brief Extract deal state from given BridgeEngine
 *
 * \param engine the engine
 * \param player the player whose deal state to extract
 *
 * \return DealState object describing the state of the deal as seen by \p
 * player
 *
 * \deprecated See DealState for more information
 */
DealState makeDealState(const BridgeEngine& engine, const Player& player);

}
}

#endif // ENGINE_MAKEGAMESTATE_HH_
