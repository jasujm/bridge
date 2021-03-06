/** \file
 *
 * \brief Definition of Bridge::CardServer::CardServerMain
 */

#ifndef CARDSERVER_CARDSERVERMAIN_HH_
#define CARDSERVER_CARDSERVERMAIN_HH_

#include "messaging/Security.hh"
#include "messaging/Sockets.hh"

#include <memory>
#include <optional>
#include <string>

namespace Bridge {

/** \brief Mental card game framework
 *
 * The classes in the CardServer namespace are needed to run the \ref
 * cardserverprotocol. The entry point for instantiating a card server is the
 * CardServerMain class.
 */
namespace CardServer {

/** \brief Entry point for a card server instance
 *
 * The CardServerMain is responsible for setting up a single card server
 * instance. A card server is a service intended to execute secure mental card
 * game protocol built on top of LibTMCG (http://nognu.org/libtmcg/).
 *
 * The card server itself is oblivious to the rules of the contract bridge or
 * any other card game. It is the responsibility of the controlling application
 * to use the commands according to the rules of the game.
 *
 * CardServerMain starts processing messages when run() is called. It
 * communicates with other peers and the controlling instance by ZeroMQ
 * messages.
 *
 * \sa \ref cardserverprotocol
 */
class CardServerMain {
public:

    /** \brief Create new card server
     *
     * \param context the ZeroMQ context
     * \param keys the CurveZMQ keys used for connections, or nullopt if curve
     * security isn’t used
     * \param controlEndpoint the endpoint the card server binds for connections
     * from the controlling application
     * \param peerEndpoint the endpoint the card server binds for
     * connections from other card server peers
     * \param securityParameter The security parameter for LibTMCG
     * (see https://www.nongnu.org/libtmcg/libTMCG.html/SchindelhauerTMCG.html#SchindelhauerTMCG)
     */
    CardServerMain(
        Messaging::MessageContext& context,
        std::optional<Messaging::CurveKeys> keys,
        std::string_view controlEndpoint,
        std::string_view peerEndpoint,
        long unsigned int securityParameter);

    ~CardServerMain();

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

#endif // CARDSERVER_CARDSERVERMAIN_HH_
