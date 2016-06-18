/** \file
 *
 * \brief Definition of Bridge::Main::CardProtocol interface
 */

#ifndef MAIN_CARDPROTOCOL_HH_
#define MAIN_CARDPROTOCOL_HH_

#include "messaging/MessageQueue.hh"

#include <boost/range/any_range.hpp>

#include <memory>

namespace Bridge {

namespace Engine {
class CardManager;
}

namespace Main {

/** \brief Interface for card protocol
 *
 * The purpose of CardProtocol interface is to abstract away the details of
 * card exchange protocol between peers.
 */
class CardProtocol {
public:

    /** \brief Return value of getHandlers()
     */
    using MessageHandlerRange = boost::any_range<
        Messaging::MessageQueue::HandlerMap::value_type,
        boost::single_pass_traversal_tag>;

    virtual ~CardProtocol();

    /** \brief Get message handlers necessary for executing the protocol
     *
     * \return A range of command‐message handler pairs that should be added
     * to a message queue handling messages from the peers.
     */
    MessageHandlerRange getMessageHandlers();

    /** \brief Get pointer to the card manager
     *
     * \return A valid pointer to a card manager that can be used to access
     * the cards.
     */
    std::shared_ptr<Engine::CardManager> getCardManager();

private:

    /** \brief Handle for returning the message handlers required for the
     * protocol
     *
     * \sa getMessageHandlers()
     */
    virtual MessageHandlerRange handleGetMessageHandlers() = 0;

    /** \brief Handler for returning the card manager of the protocol
     *
     * \sa getCardManager()
     */
    virtual std::shared_ptr<Engine::CardManager> handleGetCardManager() = 0;
};

}
}

#endif // MAIN_CARDPROTOCOL_HH_
