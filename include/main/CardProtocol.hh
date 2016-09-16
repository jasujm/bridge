/** \file
 *
 * \brief Definition of Bridge::Main::CardProtocol interface
 */

#ifndef MAIN_CARDPROTOCOL_HH_
#define MAIN_CARDPROTOCOL_HH_

#include "messaging/MessageLoop.hh"
#include "messaging/MessageQueue.hh"

#include <boost/range/any_range.hpp>
#include <zmq.hpp>

#include <memory>
#include <string>
#include <vector>

namespace Bridge {

enum class Position;

namespace Engine {
class CardManager;
}

namespace Main {

/** \brief Interface for card protocol
 *
 * The purpose of CardProtocol interface is to abstract away the details of
 * card exchange protocol between peers. It provides necessary message
 * handlers and additional sockets for performing the necessary actions of the
 * protocol.
 *
 * Any implementation needs to at least provide message handler for the peers
 * to initiate the connection.
 */
class CardProtocol {
public:

    /** \brief Vector containing positions
     */
    using PositionVector = std::vector<Position>;

    /** \brief Enumeration describing if a peer was accepted by PeerAcceptor
     */
    enum class PeerAcceptState {
        /** \brief Announce that the peer was rejected
         */
        REJECTED,

        /** \brief Announce that the peer was accepted
         */
        ACCEPTED,

        /** \brief Announce that the peer was accepted and all positions are
         * controlled
         */
        ALL_ACCEPTED
    };

    /** \brief Interface for a class accepting peers
     */
    class PeerAcceptor {
    public:

        virtual ~PeerAcceptor();

        /** \brief Accept peer
         *
         * \param identity the identity of the peer joining
         * \param positions the positions the peer wants to controls
         *
         * \return PeerAcceptState value indicating whether or not the peer
         * was accepted
         */
        virtual PeerAcceptState acceptPeer(
            const std::string& identity, const PositionVector& positions) = 0;
    };

    /** \brief Return value of getMessageHandlers()
     */
    using MessageHandlerRange = boost::any_range<
        Messaging::MessageQueue::HandlerMap::value_type,
        boost::single_pass_traversal_tag>;

    /** \brief Return value of getSockets()
     */
    using SocketRange = boost::any_range<
        std::pair<
            std::shared_ptr<zmq::socket_t>,
            Messaging::MessageLoop::SocketCallback>,
        boost::single_pass_traversal_tag>;

    virtual ~CardProtocol();

    /** \brief Set peer acceptor
     *
     * Each implementation should at least provide a message handler for
     * letting peers join. Accepting peer is delegated to PeerAcceptor object
     * which has responsibility of accepting or rejecting the peer.
     *
     * \note Each CardProtocol implementation may assume that the application
     * does not control any position that any accepted peer controls. It can
     * be assumed that any position not controlled by any peer when all peers
     * are accepted is controlled by the application itself.
     */
    void setAcceptor(std::weak_ptr<PeerAcceptor> acceptor);

    /** \brief Get message handlers necessary for executing the protocol
     *
     * \note The implementation should at least implement a message handler
     * for letting peers join the game. Client of this class can register peer
     * acceptor using setAcceptor().
     *
     * \return A range of command–message handler pairs that should be added
     * to a message queue handling messages from the peers.
     */
    MessageHandlerRange getMessageHandlers();

    /** \brief Get additional sockets that need to be polled
     *
     * \return A range containing pairs of sockets and callbacks. These
     * sockets need to be polled in message loop and incoming messages
     * signalled by calling the callback.
     *
     * \sa Messaging::MessageLoop
     */
    SocketRange getSockets();

    /** \brief Get pointer to the card manager
     *
     * \note The card protocol may require that all peers are accepted before
     * proceeding to the first shuffle request. The protocol implementation is
     * required to ensure that once the shuffle request is completed, at least
     * the cards owned by the players controlled by the application itself are
     * known.
     *
     * \return A valid pointer to a card manager that can be used to access
     * the cards.
     */
    std::shared_ptr<Engine::CardManager> getCardManager();

private:

    /** \brief Handle setting peer acceptor
     *
     * \sa setAcceptor()
     */
    virtual void handleSetAcceptor(std::weak_ptr<PeerAcceptor> acceptor) = 0;

    /** \brief Handle for returning the message handlers required for the
     * protocol
     *
     * \sa getMessageHandlers()
     */
    virtual MessageHandlerRange handleGetMessageHandlers() = 0;

    /** \brief Handle for returning sockets required for the protocol
     *
     * \sa getSockets()
     */
    virtual SocketRange handleGetSockets() = 0;

    /** \brief Handler for returning the card manager of the protocol
     *
     * \sa getCardManager()
     */
    virtual std::shared_ptr<Engine::CardManager> handleGetCardManager() = 0;
};

}
}

#endif // MAIN_CARDPROTOCOL_HH_
