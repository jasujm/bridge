/** \file
 *
 * \brief Definition of Bridge::CardServer::CardServerMain
 */

#ifndef CARDSERVER_CARDSERVERMAIN_HH_
#define CARDSERVER_CARDSERVERMAIN_HH_

#include <boost/core/noncopyable.hpp>
#include <zmq.hpp>

#include <memory>
#include <string>

namespace Bridge {

/** \brief Mental card game framework
 *
 * The main class is CardServerMain is responsible for configuring a card
 * server to execute secure mental card game protocol built on top of LibTMCG
 * (http://nognu.org/libtmcg/).
 */
namespace CardServer {

/** \brief Server for executing mental card game protocol
 *
 * This class is used to configure a card server instance. The responsibility
 * of a card server is to execute mental card game protocol between peers to
 * securely perform services such as shuffling and revealing cards. A card
 * server is oblivious to the rules of bridge or any other card game. It is
 * the responsibility of the client to use commands according to the rules of
 * the game.
 *
 * CardServerMain reserves a thread by blocking when run() is called. It
 * communicates with other peers and the client by ZeroMQ messages.
 *
 * \sa \ref bridgeprotocol
 */
class CardServerMain {
public:

    /** \brief Create new card server
     *
     * \param context ZeroMQ context
     * \param controlEndpoint the endpoint for the client to connect to
     * \param basePeerEndpoint the base endpoint the first peer connects to
     */
    CardServerMain(
        zmq::context_t& context, const std::string& controlEndpoint,
        const std::string& basePeerEndpoint);

    ~CardServerMain();

    /** \brief Run the card server
     *
     * This method blocks until terminate() is called.
     */
    void run();

    /** \brief Terminate the card server
     *
     * This method is intended to be called from signal handler for clean
     * termination.
     */
    void terminate();

private:

    class Impl;
    const std::unique_ptr<Impl> impl;
};


}
}

#endif // CARDSERVER_CARDSERVERMAIN_HH_
