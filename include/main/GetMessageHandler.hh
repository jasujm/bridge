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

namespace Engine {
class BridgeEngine;
class DuplicateGameManager;
}

namespace Main {

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
     * \param gameManager Game manager managing the game being played
     * \param engine The engine for the game being played
     * \param nodePlayerControl The node player control object used to identify
     * the node
     */
    GetMessageHandler(
        std::shared_ptr<const Engine::DuplicateGameManager> gameManager,
        std::shared_ptr<const Engine::BridgeEngine> engine,
        std::shared_ptr<const NodePlayerControl> nodePlayerControl);

private:

    bool doHandle(
        const std::string& identity, const ParameterVector& params,
        OutputSink sink) override;

    bool internalContainsKey(
        const std::string& key, const std::string& expected,
        OutputSink& sink) const;

    const std::shared_ptr<const Engine::DuplicateGameManager> gameManager;
    const std::shared_ptr<const Engine::BridgeEngine> engine;
    const std::shared_ptr<const NodePlayerControl> nodePlayerControl;
};

}
}

#endif // MAIN_GETMESSAGEHANDLER_HH_
