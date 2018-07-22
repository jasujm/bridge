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
 * Each node in the bridge protocol is allowed to control one or multiple
 * players in each game. This class is used for the access control.
 */
class NodePlayerControl : private boost::noncopyable {
public:

    /** \brief Create new node player control object
     */
    NodePlayerControl();

    ~NodePlayerControl();

    /** \brief Create player controlled by a node
     *
     * This method is used request a new player to be controlled by \p node and
     * identified by \p uuid. The combination of \p node and \p uuid can later
     * be used to check if the node is allowed to act for the player.
     *
     * The method returns nullptr if \p uuid was already used to create a
     * player.
     *
     * \param node the identity of the node controlling or representing the
     * player
     * \param uuid UUID of the new player
     */
    std::shared_ptr<Player> createPlayer(
        const Messaging::Identity& node, const std::optional<Uuid>& uuid);

    /** \brief Get player if the node is allowed to act for itself
     *
     * This method is used to check if \p node is allowed to act for a player
     * identified by \p uuid. If a player was previously created with the same
     * parameters, a pointer pointing to it is returned. Otherwise nullptr is
     * returned.
     *
     * The \p uuid parameter may also be none, in which case pointer to a valid
     * pointer is returned if and only if \p node controls exactly one player.
     *
     * \param node the identity of the node controlling or representing the
     * player
     * \param uuid UUID of the player
     */
    std::shared_ptr<const Player> getPlayer(
        const Messaging::Identity& node,
        const std::optional<Uuid>& uuid = std::nullopt) const;

private:

    class Impl;
    const std::unique_ptr<Impl> impl;
};


}
}

#endif // MAIN_NODEPLAYERCONTROL_HH_
