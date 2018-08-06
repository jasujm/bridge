/** \file
 *
 * \brief Definition of Bridge::Main::CardServerProxy
 */

#ifndef MAIN_CARDSERVERPROXY_HH_
#define MAIN_CARDSERVERPROXY_HH_

#include "main/CardProtocol.hh"

#include <boost/core/noncopyable.hpp>
#include <zmq.hpp>

#include <memory>
#include <string>

namespace Bridge {

enum class Position;

namespace Messaging {

struct CurveKeys;

}

namespace Main {

/** \brief Client for card server
 *
 * CardServerProxy implements the necessary message handlers for communicating
 * with the card server. It allows several application instances to exchange
 * cards using secure protocol with minimum probability of any of the peers
 * cheating.
 *
 * CardServerProxy requires that all the peers have joined and are accepted
 * before proceeding to the first shuffle. In addition, each peer is required to
 * provide the base endpoint for its card server peers. See \ref
 * cardserverprotocol and \ref bridgeprotocol for details.
 *
 * \sa CardServer, makePeerArgsForCardServerProxy()
 */
class CardServerProxy : public CardProtocol, private boost::noncopyable {
public:

    /** \brief Create card server proxy
     *
     * \param context the ZeroMQ context
     * \param keys the CurveZMQ keys used for connections, or nullptr if curve
     * security isn’t used
     * \param controlEndpoint the control endpoint of the card server
     */
    CardServerProxy(
        zmq::context_t& context, const Messaging::CurveKeys* keys,
        const std::string& controlEndpoint);

    class Impl;

private:

    bool handleAcceptPeer(
        const Messaging::Identity& identity, const PositionVector& positions,
        const OptionalArgs& args) override;

    void handleInitialize() override;

    MessageHandlerVector handleGetMessageHandlers() override;

    SocketVector handleGetSockets() override;

    std::shared_ptr<Engine::CardManager> handleGetCardManager() override;

    const std::shared_ptr<Impl> impl;
};

/** \brief Make required additional arguments for CardServerProxy::acceptPeer()
 *
 * This function creates the necessary additional arguments that CardServerProxy
 * needs when accepting a peer. The arguments contain the base endpoint for the
 * card server peers.
 *
 * \param endpoint the base peer endpoint of the card server the peer uses
 *
 * \return the additional arguments as JSON object
 */
nlohmann::json makePeerArgsForCardServerProxy(const std::string& endpoint);

}
}

#endif // MAIN_CARDSERVERPROXY_HH_
