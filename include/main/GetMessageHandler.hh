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
     * The function is expected to take UUID of the game as argument and return
     * pointer BridgeGameInfo object for it, or nullptr if there is no game with
     * the UUID.
     *
     * The pointer is not stored so the lifetime of the object does not need to
     * extend after the caller has returned.
     *
     * \sa GetMessageHandler()
     */
    using GetGameFunction = std::function<const BridgeGameInfo*(const Uuid&)>;

    /** \brief Create new get message handler
     *
     * \param games Function for retrieving BridgeGameInfo object
     * \param nodePlayerControl The node player control object used to identify
     * the node
     */
    GetMessageHandler(
        GetGameFunction games,
        std::shared_ptr<const NodePlayerControl> nodePlayerControl);

    /** \brief Get list on all supported keys
     *
     * \return List of all keys that can be used in the query, i.e. the keys
     * listed in \ref bridgeprotocolcontrolget command
     */
    static std::vector<std::string> getAllKeys();

private:

    bool doHandle(
        const Messaging::Identity& identity, const ParameterVector& params,
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
