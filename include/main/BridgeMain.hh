/** \file
 *
 * \brief Definition of Bridge::Main::BridgeMain class
 */

#ifndef MAIN_BRIDGEMAIN_HH_
#define MAIN_BRIDGEMAIN_HH_

#include <zmq.hpp>

#include <memory>

namespace Bridge {

/** \brief The glue code and high level logic for the bridge backend
 *
 * The main class BridgeMain is responsible for setting up the Bridge backend
 * application.
 */
namespace Main {

class Config;

/** \brief Set up and run the Bridge backend
 *
 * When constructed, BridgeMain configures and sets up Bridge peer application,
 * including the sockets for communicating with clients and other peers. Two
 * sockets are opened into consecutive ports: the control socket and the event
 * socket.
 *
 * The backend starts processing messages when run() is called. The destructor
 * closes sockets and cleans up the application.
 *
 * \sa \ref bridgeprotocol
 */
class BridgeMain {
public:

    /** \brief Create Bridge backend
     *
     * \param context the ZeroMQ context for the game
     * \param config the application configurations
     */
    BridgeMain(zmq::context_t& context, Config config);

    ~BridgeMain();

    /** \brief Start receiving and handling messages
     *
     * This method blocks until SIGINT or SIGTERM is received.
     */
    void run();

private:

    class Impl;
    const std::unique_ptr<Impl> impl;
};

}
}

#endif // MAIN_BRIDGEMAIN_HH_
