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
 * has a control socket meant for user interaction. It also connects to the
 * instances of other Bridge appliations through several other sockets.
 */
class BridgeMain : private boost::noncopyable {
public:

    /** \brief Command for requesting deal state
     */
    static const std::string STATE_COMMAND;

    /** \brief Command for making a call during bidding
     */
    static const std::string CALL_COMMAND;

    /** \brief Command for playing a card during playing phase
     */
    static const std::string PLAY_COMMAND;

    /** \brief Command for requesting scoresheet
     */
    static const std::string SCORE_COMMAND;

    /** \brief Create bridge main
     *
     * The constructor creates the necessary classes and starts the game.
     *
     * \param context the ZeroMQ context for the game
     * \param address the ZeroMQ address the control socket binds to
     */
    BridgeMain(zmq::context_t& context, const std::string& address);

    ~BridgeMain();

private:

    class Impl;
    const std::unique_ptr<Impl> impl;
};

}
}

#endif // MAIN_BRIDGEMAIN_HH_
