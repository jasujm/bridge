/** \file
 *
 * \brief Definition of Bridge::Main::BridgeMain class
 */

#ifndef MAIN_BRIDGEMAIN_HH_
#define MAIN_BRIDGEMAIN_HH_

#include "bridge/Position.hh"

#include <zmq.hpp>

#include <deque>
#include <memory>
#include <string>
#include <vector>

namespace Bridge {

/** \brief The main configuration and high level logic for the bridge backend
 *
 * The main class BridgeMain is responsible for configuring the application
 * communicating with peers and clients using the \ref bridgeprotocol.
 */
namespace Main {

/** \brief Configure and run the event loop for the Bridge backend
 *
 * When constructed, BridgeMain configures and sets up Bridge peer application
 * and sockets for communicating with clients and other peers. Two sockets are
 * opened into consecutive ports: the control socket and the event socket.
 *
 * The backend starts processing and publishing messages when run() is
 * called. The destructor closes sockets and cleans up the application.
 *
 * \sa \ref bridgeprotocol
 */
class BridgeMain {
public:

    /** \brief Parameter to BridgeMain()
     */
    using PositionVector = std::vector<Position>;

    /** \brief Parameter for BridgeMain()
     */
    using EndpointVector = std::vector<std::string>;

    /** \brief Create Bridge backend
     *
     * \param context the ZeroMQ context for the game
     * \param baseEndpoint the base endpoint for control and event sockets
     * \param positionsControlled the positions controlled by the application
     * \param peerEndpoints the base endpoints of the peers
     * \param cardServerControlEndpoint the control endpoint of a card server
     * the application connects
     * \param cardServerBasePeerEndpoint the base endpoint used by the peers
     * to establish the peer connection
     *
     * \note If both \p cardServerControlEndpoint and \p
     * cardServerBasePeerEndpoint are nonempty, card server is used for card
     * exchange between the peers. The given parameters are used to connect
     * the server and the peers. If either one is empty, the card server
     * endpoints are ignored and the simple cleartext protocol is used instead.
     *
     * \note The control socket is bound to \p baseEndpoint. The event socket
     * is bound to the same interface but one larger port.
     */
    BridgeMain(
        zmq::context_t& context,
        const std::string& baseEndpoint,
        const PositionVector& positionsControlled,
        const EndpointVector& peerEndpoints,
        const std::string& cardServerControlEndpoint,
        const std::string& cardServerBasePeerEndpoint);

    ~BridgeMain();

    /** \brief Run the backend message queue
     *
     * This method blocks until terminate() is called.
     */
    void run();

    /** \brief Terminate the backend message queue
     *
     * This method is intended to be called from signal handler for clean
     * termination.
     */
    void terminate();

private:

    class Impl;
    std::shared_ptr<Impl> impl;
};

}
}

#endif // MAIN_BRIDGEMAIN_HH_
