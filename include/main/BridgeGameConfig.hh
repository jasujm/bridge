#ifndef MAIN_BRIDGEGAMECONFIG_HH_
#define MAIN_BRIDGEGAMECONFIG_HH_

#include "bridge/Uuid.hh"

namespace Bridge {
namespace Main {

/** \brief Bridge game configuration info
 *
 * An object containing the necessary configuration data needed to set up a
 * bridge game. The intention is that BridgeGameConfig is used as an
 * intermediate representation of the game coming from config file or other
 * source, and that will be used as a template to create a functioning
 * BridgeGame object.
 */
struct BridgeGameConfig {
    Uuid uuid;
};

}
}

#endif // MAIN_BRIDGEGAMECONFIG_HH_
