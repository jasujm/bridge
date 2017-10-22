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

    /** \brief Function used to retrieve game information
     *
     * The function is expected to accept UUID of the game and return
     * BridgeGameInfo object for it, or nullptr if there is no game with the
     * UUID.
     *
     * \sa GetMessageHandler()
     */
    using GetGameFunction = std::function<
        std::shared_ptr<const BridgeGameInfo>(const Uuid&)>;

    /** \brief Create new get message handler
     *
     * \param games Function for retrieving BridgeGameInfo object
     * \param nodePlayerControl The node player control object used to identify
     * the node
     */
    GetMessageHandler(
        GetGameFunction games,
        std::shared_ptr<const NodePlayerControl> nodePlayerControl);

private:

    bool doHandle(
        const std::string& identity, const ParameterVector& params,
        OutputSink sink) override;

    bool internalContainsKey(
        const std::string& key, const std::string& expected,
        OutputSink& sink) const;

    const GetGameFunction games;
    const std::shared_ptr<const NodePlayerControl> nodePlayerControl;
};

}
}

#endif // MAIN_GETMESSAGEHANDLER_HH_
