/** \file
 *
 * \brief Definition of Bridge::CardServer::PeerSocketProxy
 */

#ifndef BRIDGE_CARDSERVER_PEERSOCKETPROXY_HH_
#define BRIDGE_CARDSERVER_PEERSOCKETPROXY_HH_

#include "messaging/MessageLoop.hh"

#include <zmq.hpp>

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
class PeerSocketProxy {
public:

    /** \brief Vector consisting of sockets
     */
    using SocketVector = std::vector<std::shared_ptr<zmq::socket_t>>;

    /** \brief Vector consisting of socket–callback pairs
     */
    using SocketCallbackVector = std::vector<
        std::pair<
            std::shared_ptr<zmq::socket_t>,
            Messaging::MessageLoop::SocketCallback>>;

    /** \brief Order parameter of proxied messages
     */
    using OrderParameter = std::uint8_t;

    /** \brief Create peer socket proxy
     *
     * \param context The ZeroMQ context
     * \param peerServerSocket The ROUTER socket used to receive messages from
       the peers
     * \param peerClientSockets The DEALER sockets used to send messages to the
     * peers. The size of the vector determines the total number of peers.
     * \param selfOrder The order parameter of self
     */
    PeerSocketProxy(
        zmq::context_t& context, zmq::socket_t peerServerSocket,
        std::vector<zmq::socket_t> peerClientSockets, OrderParameter selfOrder);

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

    void internalHandleMessageFromPeer(zmq::socket_t& socket);

    void internalHandleMessageToPeer(
        zmq::socket_t& streamSocket, zmq::socket_t& clientSocket);

    OrderParameter selfOrder;
    std::shared_ptr<zmq::socket_t> peerServerSocket;
    SocketVector peerClientSockets;
    SocketVector frontStreamSockets;
    SocketVector streamSockets;
};

}
}

#endif // BRIDGE_CARDSERVER_PEERSOCKETPROXY_HH_
