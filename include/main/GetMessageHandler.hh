/** \file
 *
 * \brief Definition of Bridge::Main::GetMessageHandler class
 */

#ifndef MAIN_GETMESSAGEHANDLER_HH_
#define MAIN_GETMESSAGEHANDLER_HH_

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
     * \param gameInfo The bridge game info object used to query information
     * about the game
     * \param nodePlayerControl The node player control object used to identify
     * the node
     */
    GetMessageHandler(
        std::shared_ptr<const BridgeGameInfo> gameInfo,
        std::shared_ptr<const NodePlayerControl> nodePlayerControl);

private:

    bool doHandle(
        const std::string& identity, const ParameterVector& params,
        OutputSink sink) override;

    bool internalContainsKey(
        const std::string& key, const std::string& expected,
        OutputSink& sink) const;

    const std::shared_ptr<const BridgeGameInfo> gameInfo;
    const std::shared_ptr<const NodePlayerControl> nodePlayerControl;
};

}
}

#endif // MAIN_GETMESSAGEHANDLER_HH_
