/** \file
 *
 * \brief Definition of Bridge::Main::CardProtocol interface
 */

#ifndef MAIN_CARDPROTOCOL_HH_
#define MAIN_CARDPROTOCOL_HH_

#include "messaging/MessageLoop.hh"
#include "messaging/MessageQueue.hh"

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

    /** \brief Return value of getMessageHandlers()
     */
    using MessageHandlerVector = std::vector<
        Messaging::MessageQueue::HandlerMap::value_type>;

    /** \brief Return value of getSockets()
     */
    using SocketVector = std::vector<
        std::pair<
            std::shared_ptr<zmq::socket_t>,
            Messaging::MessageLoop::SocketCallback>>;

    virtual ~CardProtocol();

    /** \brief Accept peer
     *
     * This method is used to accept or reject a peer to take part in the card
     * exchange protocol managed by the CardProtocol instance. If the peer is
     * accepted, this method returns true. If the peer is rejected, this method
     * returns false, in which case the method call has no effect.
     *
     * \param identity the identity of the peer
     * \param positions the positions the peer requests to represent
     *
     * \return true if the peer is accepted, false if rejected
     */
    bool acceptPeer(
        const std::string& identity, const PositionVector& positions);

    /** \brief Initialize the protocol
     *
     * This method is called by the client of the CardProtocol instance after
     * all peers taking part in the card exchange have been accepted. The
     * derived class may assume that at that point all positions not yet
     * controlled by any of the peers are controlled by the application itself.
     */
    void initialize();

    /** \brief Get message handlers necessary for executing the protocol
     *
     * \return A vector of command–message handler pairs that should be added
     * to a message queue handling messages from the peers.
     */
    MessageHandlerVector getMessageHandlers();

    /** \brief Get additional sockets that need to be polled
     *
     * \return A vector containing pairs of sockets and callbacks. These
     * sockets need to be polled in message loop and incoming messages
     * signalled by calling the callback.
     *
     * \sa Messaging::MessageLoop
     */
    SocketVector getSockets();

    /** \brief Get pointer to the card manager
     *
     * \note The card protocol may require that initialize() has been called
     * before the first shuffle is requested. The protocol implementation is
     * required to ensure that once the shuffle request is completed, at least
     * the cards owned by the players controlled by the application itself are
     * known.
     *
     * \return A valid pointer to a card manager that can be used to access
     * the cards.
     */
    std::shared_ptr<Engine::CardManager> getCardManager();

private:

    /** \brief Handle for accepting peer
     *
     * \sa acceptPeer()
     */
    virtual bool handleAcceptPeer(
        const std::string& identity, const PositionVector& positions) = 0;

    /** \brief Handle initializing the protocol
     *
     * \sa initialize()
     */
    virtual void handleInitialize() = 0;

    /** \brief Handle for returning the message handlers required for the
     * protocol
     *
     * \sa getMessageHandlers()
     */
    virtual MessageHandlerVector handleGetMessageHandlers() = 0;

    /** \brief Handle for returning sockets required for the protocol
     *
     * \sa getSockets()
     */
    virtual SocketVector handleGetSockets() = 0;

    /** \brief Handler for returning the card manager of the protocol
     *
     * \sa getCardManager()
     */
    virtual std::shared_ptr<Engine::CardManager> handleGetCardManager() = 0;
};

}
}

#endif // MAIN_CARDPROTOCOL_HH_
