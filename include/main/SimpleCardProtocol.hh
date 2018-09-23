/** \file
 *
 * \brief Definition of Bridge::Main::SimpleCardProtocol
 */

#ifndef MAIN_SIMPLECARDPROTOCOL_HH_
#define MAIN_SIMPLECARDPROTOCOL_HH_

#include "bridge/Uuid.hh"
#include "main/CardProtocol.hh"

#include <boost/core/noncopyable.hpp>

#include <memory>

namespace Bridge {
namespace Main {

class PeerCommandSender;

/** \brief Simple plaintext card protocol
 *
 * SimpleCardProtocol implements a simple plaintext card protocol where one
 * player generates the cards and sends them to all, unencrypted. This protocl
 * should only be used between trusted parties.
 *
 * \todo Dispatching commands to correct protocol is based on static mapping
 * between game UUIDs and SimpleCardProtocol instances. The users of
 * SimpleCardProtocol have very little control of this mapping, or error
 * handling related to UUID collisions. More transparent interface for
 * dispatching is required.
 */
class SimpleCardProtocol : public CardProtocol, private boost::noncopyable {
public:

    /** \brief Create simple card manager
     *
     * \param gameUuid UUID of the game owning this protocol
     * \param peerCommandSender A peer command sender that can be used to send
     * commands to the peers.
     */
    SimpleCardProtocol(
        const Uuid& gameUuid,
        std::shared_ptr<PeerCommandSender> peerCommandSender);

    ~SimpleCardProtocol();

private:

    bool handleAcceptPeer(
        const Messaging::Identity& identity, const PositionVector& positions,
        const OptionalArgs& args) override;

    void handleInitialize() override;

    MessageHandlerVector handleGetMessageHandlers() override;

    SocketVector handleGetSockets() override;

    std::shared_ptr<Engine::CardManager> handleGetCardManager() override;

    class DealCommandDispatcher;
    static DealCommandDispatcher dispatcher;

    class Impl;
    const std::shared_ptr<Impl> impl;
};


}
}

#endif // MAIN_SIMPLECARDPROTOCOL_HH_
