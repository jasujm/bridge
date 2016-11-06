/** \file
 *
 * \brief Definition of Bridge::Main::SimpleCardProtocol
 */

#ifndef MAIN_SIMPLECARDPROTOCOL_HH_
#define MAIN_SIMPLECARDPROTOCOL_HH_

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
 */
class SimpleCardProtocol : public CardProtocol, private boost::noncopyable {
public:

    /** \brief Create simple card manager
     *
     * \param peerCommandSender A peer command sender that can be used to send
     * commands to the peers.
     */
    SimpleCardProtocol(std::shared_ptr<PeerCommandSender> peerCommandSender);

private:

    bool handleAcceptPeer(
        const std::string& identity, const PositionVector& positions) override;

    void handleInitialize() override;

    MessageHandlerVector handleGetMessageHandlers() override;

    SocketVector handleGetSockets() override;

    std::shared_ptr<Engine::CardManager> handleGetCardManager() override;

    class Impl;
    const std::shared_ptr<Impl> impl;
};


}
}

#endif // MAIN_SIMPLECARDPROTOCOL_HH_
