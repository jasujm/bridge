/** \file
 *
 * \brief Definition of Bridge::Main::NodePlayerControl
 */

#ifndef MAIN_NODEPLAYERCONTROL_HH_
#define MAIN_NODEPLAYERCONTROL_HH_

#include "bridge/Player.hh"
#include "messaging/Identity.hh"

#include <boost/core/noncopyable.hpp>

#include <memory>

namespace Bridge {

class Player;

namespace Main {

class BridgeGameRecorder;

/** \brief Utility class for access control of nodes and players
 *
 * Each node in the bridge protocol is allowed to control one or more
 * players in each game. This class is used for the access control.
 */
class NodePlayerControl : private boost::noncopyable {
public:

    /** \brief Create new node player control object
     */
    NodePlayerControl();

    ~NodePlayerControl();

    /** \brief Create or retrieve a player controlled by a node
     *
     * This method is used to create or get a player to be controlled by \p node
     * and identified by \p uuid. The combination of \p node and \p uuid can
     * later be used to check if the node is allowed to act for the player.
     *
     * The method returns nullptr if \p uuid already exists and is not
     * controlled by \p node.
     *
     * \param node The identity of the node
     * \param uuid The UUID of the player to test
     * \param recorder An optional recorder for recording and recalling players
     */
    std::shared_ptr<Player> getOrCreatePlayer(
        const Messaging::UserId& node, const Uuid& uuid,
        BridgeGameRecorder* recorder);

private:

    class Impl;
    const std::unique_ptr<Impl> impl;
};


}
}

#endif // MAIN_NODEPLAYERCONTROL_HH_
