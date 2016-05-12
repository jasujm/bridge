/** \file
 *
 * \brief Definition of Bridge::Main::BridgeMain class
 *
 * \page bridgeapp Bridge application structure
 *
 * Bridge application uses client–server model where the backend application
 * acts as server for the frontend user interface. The interaction between the
 * backend and frontend happens using the protocol described in \ref
 * bridgeprotocol.
 *
 * \section bridgeprotocol Bridge protocol
 *
 * Bridge protocol uses ZMTP over TCP (http://rfc.zeromq.org/spec:23). The
 * backend opens control socket (ROUTER) for communicating commands and game
 * state with clients, and event socket (PUB) for publishing events.
 *
 * The control socket supports the following commands:
 *   - bridgehlo
 *   - bridgerp
 *   - state
 *   - deal
 *   - call
 *   - play
 *   - score
 *
 * The event socket publishes the following events:
 *   - call
 *   - deal
 *   - play
 *   - score
 *
 * \todo This section is incomplete
 */

#ifndef MAIN_BRIDGEMAIN_HH_
#define MAIN_BRIDGEMAIN_HH_

#include "bridge/Position.hh"

#include <boost/core/noncopyable.hpp>
#include <boost/range/any_range.hpp>
#include <zmq.hpp>

#include <deque>
#include <memory>

namespace Bridge {
namespace Main {

/** \brief Configure and run the event loop for the Bridge backend
 *
 * When constructed, BridgeMain configures and sets up Bridge application and
 * sockets for communicating with external frontend applications. The backend
 * starts processing and publishing messages when run() is called. The
 * destructor closes sockets and cleans up the application.
 *
 * \sa \ref bridgeapp
 */
class BridgeMain : private boost::noncopyable {
public:

    /** \brief Parameter to BridgeMain()
     */
    using PositionRange = boost::any_range<
        Position, boost::forward_traversal_tag>;

    /** \brief Parameter for BridgeMain()
     */
    using EndpointRange = boost::any_range<
        std::string, boost::single_pass_traversal_tag>;

    /** \brief Handshake command for joining the game as client
     *
     * If the client can join, it is added to the list of players and the
     * reply to this command is success. Otherwise the reply is failure.
     */
    static const std::string HELLO_COMMAND;

    /** \brief Handshake command for joining the game as peer
     *
     * If the peer can join, it is added to the list of peers and the reply to
     * this command is success. Otherwise the reply is failure.
     */
    static const std::string PEER_COMMAND;

    /** \brief Command for dealing cards to peers
     *
     * The leader peer uses this command to deal cards to other peers after
     * generating the shuffled deck. The command is successful if it comes
     * from the leader and the peer is expecting cards. Otherwise it is
     * failure.
     */
    static const std::string DEAL_COMMAND;

    /** \brief Command for requesting deal state
     *
     * The reply to this command contains the deal state.
     */
    static const std::string STATE_COMMAND;

    /** \brief Command for making a call during bidding
     *
     * The Call serialized into JSON is sent as the second part of the
     * message. The reply to this command is success if the sender has joined
     * the game, false otherwise.
     */
    static const std::string CALL_COMMAND;

    /** \brief Command for playing a card during playing phase
     *
     * The CardType serialized into JSON is sent as the second part of the
     * message. The reply to this command is success if the sender has joined
     * the game, false otherwise.
     */
    static const std::string PLAY_COMMAND;

    /** \brief Command for requesting score sheet
     *
     * The reply to this command contains the score sheet.
     */
    static const std::string SCORE_COMMAND;

    /** \brief Create Bridge backend
     *
     * \param context the ZeroMQ context for the game
     * \param controlEndpoint the endpoint for the control socket
     * \param eventEndpoint the endpoint for the event socket
     * \param positionsControlled the positions controlled by the application
     * \param peerEndpoints the endpoints of the peers to connect to
     */
    BridgeMain(
        zmq::context_t& context,
        const std::string& controlEndpoint,
        const std::string& eventEndpoint,
        PositionRange positionsControlled,
        EndpointRange peerEndpoints);

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
    const std::shared_ptr<Impl> impl;
};

}
}

#endif // MAIN_BRIDGEMAIN_HH_
