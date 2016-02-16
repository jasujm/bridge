/** \file
 *
 * \brief Definition of Bridge::Main::BridgeMain class
 */

#ifndef MAIN_BRIDGEMAIN_HH_
#define MAIN_BRIDGEMAIN_HH_

#include <boost/core/noncopyable.hpp>
#include <zmq.hpp>

#include <memory>

namespace Bridge {
namespace Main {

/** \brief Main business logic of the Bridge application
 *
 * BridgeMain is used to configure and mediate the components of which the
 * main functionality of the bridge application consists of. In its
 * constructor it spawns the main application thread, which is joined at the
 * destructor.
 *
 * Communication with the thread is handled through ZeroMQ socket. BridgeMain
 * has a control socket (REP) to accept commands from the user, and data
 * socket (PUB) to publish the state of the game.
 */
class BridgeMain : private boost::noncopyable {
public:

    /** \brief Command for requesting deal state
     *
     * When state command is received, the current deal state serialized into
     * JSON is sent through the data socket with STATE_PREFIX.
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
     * The reply to successful score command contains the score sheet
     * serialized into JSON.
     */
    static const std::string SCORE_COMMAND;

    /** \brief Prefix used by the data socket to publish state
     */
    static const std::string STATE_PREFIX;

    /** \brief Create bridge main
     *
     * The constructor creates the necessary classes and starts the game.
     *
     * \param context the ZeroMQ context for the game
     * \param controlEndpoint the endpoint for the control socket
     * \param dataEndpoint the endpoint for the data socket
     */
    BridgeMain(
        zmq::context_t& context, const std::string& controlEndpoint,
        const std::string& dataEndpoint);

    ~BridgeMain();

private:

    class Impl;
    const std::unique_ptr<Impl> impl;
};

}
}

#endif // MAIN_BRIDGEMAIN_HH_
