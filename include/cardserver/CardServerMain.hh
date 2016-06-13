/** \file
 *
 * \brief Definition of Bridge::CardServer::CardServerMain
 */

#ifndef BRIDGE_CARDSERVERMAIN_HH_
#define BRIDGE_CARDSERVERMAIN_HH_

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
 * \todo Write protocol specification
 */
class CardServerMain {
public:

    /** \brief Command for initializing the card server
     */
    static const std::string INIT_COMMAND;

    /** \brief Command for creating new shuffled deck with peers
     */
    static const std::string SHUFFLE_COMMAND;

    /** \brief Command for drawing a card
     */
    static const std::string DRAW_COMMAND;

    /** \brief Command for revealing cards to the peers
     */
    static const std::string REVEAL_COMMAND;

    /** \brief Command for terminating the card server
     */
    static const std::string TERMINATE_COMMAND;

    /** \brief Create new card server
     *
     * \param context ZeroMQ context
     * \param controlEndpoint the endpoint for the client to connect to
     */
    CardServerMain(
        zmq::context_t& context, const std::string& controlEndpoint);

    ~CardServerMain();

    /** \brief Run the card server
     *
     * This method blocks until terminated using \ref TERMINATE_COMMAND.
     */
    void run();

private:

    class Impl;
    const std::unique_ptr<Impl> impl;
};


}
}

#endif // BRIDGE_CARDSERVERMAIN_HH_
