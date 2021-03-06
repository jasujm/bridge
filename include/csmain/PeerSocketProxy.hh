/** \file
 *
 * \brief Definition of Bridge::CardServer::PeerSocketProxy
 */

#ifndef BRIDGE_CARDSERVER_PEERSOCKETPROXY_HH_
#define BRIDGE_CARDSERVER_PEERSOCKETPROXY_HH_

#include "messaging/Identity.hh"
#include "messaging/MessageLoop.hh"
#include "messaging/Sockets.hh"

#include <boost/core/noncopyable.hpp>

#include <cstdint>
#include <functional>
#include <memory>
#include <vector>

namespace Bridge {
namespace CardServer {

/** \brief Proxies messages to/from card server peers
 *
 * This class exists to adapt the external card server peer socket interface (a
 * single ZeroMQ ROUTER socket) to the stream based interface expected by the
 * LibTMCG interface.
 *
 * The peers are ordered by the rules explained in \ref cardserverorder. This
 * order is used to route messages between peers. Each message received by the
 * proxy socket needs to be prepended with an order parameter corresponding to
 * the peer who sent it. Similarly each outgoing message is prepended by the
 * self order. The order parameters are transparent to the stream sockets that
 * can be used by the stream based interfaces without knowing about the details
 * about order and routing.
 */
class PeerSocketProxy : private boost::noncopyable {
public:

    /** \brief Vector consisting of sockets
     */
    using SocketVector = std::vector<Messaging::SharedSocket>;

    /** \brief Vector consisting of socket–callback pairs
     */
    using SocketCallbackVector = std::vector<
        std::pair<
            Messaging::SharedSocket,
            Messaging::MessageLoop::SocketCallback>>;

    /** \brief Function used to authorize peer message
     *
     * Authorization function is a function used by PeerSocketProxy to authorize
     * an incoming message from a peer. Its parameters are the identity of the
     * peer sending the message, and the order parameter attached to the
     * message. The authorization function is expected to return true if the
     * peer is authorized to send messages with the given order parameter, and
     * false otherwise.
     */
    using AuthorizationFunction = std::function<
        bool(const Messaging::Identity&, int)>;

    /** \brief Create peer socket proxy
     *
     * \param context The ZeroMQ context
     * \param peerServerSocket The ROUTER socket used to receive messages from
       the peers
     * \param peerClientSockets The DEALER sockets used to send messages to the
     * peers. The size of the vector determines the total number of peers.
     * \param selfOrder The order parameter of self. Must be between 0—255 due
     * the wire representation of order parameter.
     * \param authorizer The authorization function used by PeerSocketProxy to
     * authorize incoming messages
     */
    PeerSocketProxy(
        Messaging::MessageContext& context, Messaging::Socket peerServerSocket,
        std::vector<Messaging::Socket> peerClientSockets, int selfOrder,
        AuthorizationFunction authorizer);

    /** \brief Get socket–callback pairs that need to be polled
     *
     * \return A vector containing pairs of sockets and callbacks. These sockets
     * need to be registered to a message loop in order to make the proxy
     * functional.
     *
     * \sa Messaging::MessageLoop
     */
    SocketCallbackVector getPollables();

    /** \brief Get stream sockets used to communicate with the peers
     *
     * Get a vector of sockets. The returned vector has one socket per
     * peer, and their order corresponds to the order of the peer sockets passed
     * to the constructor.
     *
     * \return The stream sockets proxied to the server socket passed to the
     * constructor
     */
    SocketVector getStreamSockets();

private:

    using OrderParameter = std::uint8_t;

    void internalHandleMessageFromPeer(Messaging::Socket& socket);

    void internalHandleMessageToPeer(
        Messaging::Socket& streamSocket, Messaging::Socket& clientSocket);

    const OrderParameter selfOrder;
    const AuthorizationFunction authorizer;
    const Messaging::SharedSocket peerServerSocket;
    SocketVector peerClientSockets;
    SocketVector frontStreamSockets;
    SocketVector streamSockets;
};

}
}

#endif // BRIDGE_CARDSERVER_PEERSOCKETPROXY_HH_
