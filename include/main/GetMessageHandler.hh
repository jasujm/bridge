/** \file
 *
 * \brief Definition of Bridge::Main::GetMessageHandler class
 */

#ifndef MAIN_GETMESSAGEHANDLER_HH_
#define MAIN_GETMESSAGEHANDLER_HH_

#include "bridge/Uuid.hh"
#include "messaging/MessageHandler.hh"

#include <boost/core/noncopyable.hpp>

#include <functional>
#include <memory>

namespace Bridge {
namespace Main {

class BridgeGameInfo;
class NodePlayerControl;

/** \brief Message handler for the get command
 *
 * This class is used to handle and reply to the \ref bridgeprotocolcontrolget
 * command in the \ref bridgeprotocol.
 */
class GetMessageHandler :
    public Messaging::MessageHandler, private boost::noncopyable {
public:

    /** \brief Create new get message handler
     *
     * \param game The game the info is retrieved from
     * \param nodePlayerControl The node player control object used to identify
     * the node
     */
    GetMessageHandler(
        std::weak_ptr<const BridgeGameInfo> game,
        std::shared_ptr<const NodePlayerControl> nodePlayerControl);

    /** \brief Get list on all supported keys
     *
     * \return List of all keys that can be used in the query, i.e. the keys
     * listed in \ref bridgeprotocolcontrolget command
     */
    static std::vector<std::string> getAllKeys();

private:

    void doHandle(
        ExecutionContext, const Messaging::Identity& identity,
        const ParameterVector& params, Messaging::Response& response) override;

    bool internalContainsKey(
        const std::string& key, const std::string& expected,
        Messaging::Response& response) const;

    const std::weak_ptr<const BridgeGameInfo> game;
    const std::shared_ptr<const NodePlayerControl> nodePlayerControl;
};

}
}

#endif // MAIN_GETMESSAGEHANDLER_HH_
