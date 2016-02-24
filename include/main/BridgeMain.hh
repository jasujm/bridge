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
 * backend opens two ZeroMQ sockets: control socket (REP) and data socket
 * (PUB).
 *
 * The control socket supports the following commands:
 *   - state
 *   - call
 *   - play
 *   - score
 *
 * The data socket publishes the following data:
 *   - state
 *   - score
 *
 * \todo This section is incomplete
 */

#ifndef MAIN_BRIDGEMAIN_HH_
#define MAIN_BRIDGEMAIN_HH_

#include <boost/core/noncopyable.hpp>
#include <zmq.hpp>

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

    /** \brief Command for requesting deal state
     *
     * When state command is received, the current deal state serialized into
     * JSON is sent through the data socket with \ref STATE_PREFIX.
     */
    static const std::string STATE_COMMAND;

    /** \brief Command for making a call during bidding
     *
     * The Call serialized into JSON is sent as the second part of the message.
     */
    static const std::string CALL_COMMAND;

    /** \brief Command for playing a card during playing phase
     *
     * The CardType serialized into JSON is sent as the second part of the
     * message.
     */
    static const std::string PLAY_COMMAND;

    /** \brief Command for requesting score sheet
     *
     * When score command is received, the current score sheet serialized into
     * JSON is sent through the data socket with \ref SCORE_PREFIX.
     */
    static const std::string SCORE_COMMAND;

    /** \brief Prefix used by the data socket to publish state
     */
    static const std::string STATE_PREFIX;

    /** \brief Prefix used by the data socket to publish score sheet
     */
    static const std::string SCORE_PREFIX;

    /** \brief Create Bridge backend
     *
     * \param context the ZeroMQ context for the game
     * \param controlEndpoint the endpoint for the control socket
     * \param dataEndpoint the endpoint for the data socket
     */
    BridgeMain(
        zmq::context_t& context, const std::string& controlEndpoint,
        const std::string& dataEndpoint);

    ~BridgeMain();

    /** \brief Run the backend message queue
     *
     * This method blocks until terminate() is called.
     */
    void run();

    /** \brief Terminate the backend message queue
     */
    void terminate();

private:

    class Impl;
    const std::shared_ptr<Impl> impl;
};

}
}

#endif // MAIN_BRIDGEMAIN_HH_
