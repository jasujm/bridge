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
#include <optional>

namespace Bridge {

class Player;

namespace Main {

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
     * This method is used request a new player to be controlled by \p node and
     * identified by \p uuid. The combination of \p node and \p uuid can later
     * be used to check if the node is allowed to act for the player.
     *
     * The method returns nullptr if \p uuid already exists and is not
     * controlled by \p node.
     *
     * The \p uuid parameter can be omitted, in which case an unique
     * player belonging to \p node is created or returned. If multiple
     * players have already been created for \p node, nullptr is
     * returned.
     *
     * \param node the identity of the node controlling or representing the
     * player
     * \param uuid UUID of the new player
     */
    std::shared_ptr<Player> getOrCreatePlayer(
        const Messaging::Identity& node, const std::optional<Uuid>& uuid);

private:

    class Impl;
    const std::unique_ptr<Impl> impl;
};


}
}

#endif // MAIN_NODEPLAYERCONTROL_HH_
