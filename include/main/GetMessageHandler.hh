/** \file
 *
 * \brief Definition of Bridge::Main::GetMessageHandler class
 */

#ifndef MAIN_GETMESSAGEHANDLER_HH_
#define MAIN_GETMESSAGEHANDLER_HH_

#include "main/Commands.hh"
#include "messaging/MessageHandler.hh"

#include <boost/core/noncopyable.hpp>

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

private:

    void doHandle(
        ExecutionContext, const Messaging::Identity& identity,
        const ParameterVector& params, Messaging::Response& response) override;

    const std::weak_ptr<const BridgeGameInfo> game;
    const std::shared_ptr<const NodePlayerControl> nodePlayerControl;
};

}
}

#endif // MAIN_GETMESSAGEHANDLER_HH_
