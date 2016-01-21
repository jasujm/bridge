/** \file
 *
 * \brief Definition of Bridge::Messaging::MessageQueue class
 */

#ifndef MESSAGING_MESSAGEQUEUE_HH_
#define MESSAGING_MESSAGEQUEUE_HH_

#include <zmq.hpp>

#include <map>
#include <memory>
#include <string>
#include <utility>

namespace Bridge {

/** \brief The messaging framework
 *
 * Namespace Messaging contains utilities for exchanging messages between
 * different threads and processes in the Bridge application.
 */
namespace Messaging {

/** \brief Message queue for communicating between threads and processes
 *
 * Bridge application threads are built around exchanging information and
 * requests through messages. MessageQueue is responsible for receiving and
 * dispatching the messages to handlers.
 *
 * MessageQueue uses request-reply pattern. The messages sent to the message
 * queue are strings that represent commands. A recognized command
 * (i.e. command for which a handler is registered) causes the handler for
 * that command to be executed. The reply is a message containing the string
 * returned by the handler.
 */
class MessageQueue {
public:

    /** \brief Message used to request termination
     */
    static const std::string MESSAGE_TERMINATE;

    /** \brief Message used as reply to indicate error
     */
    static const std::string MESSAGE_ERROR;

    /** \brief Command handler
     *
     * Handler is a function that, when invoked, returns reply sent to the
     * client who sent the command.
     */
    using Handler = std::function<std::string()>;

    /** \brief Create message queue
     *
     * \tparam HandlerIterator Iterator that, when dereferenced, returns a
     * pair containing the message and the \ref Handler used to react to that
     * message.
     *
     * \param first iterator to the first handler
     * \param last iterator to the last handler
     * \param socket the socket to use for communicating between the queue and
     * its clients
     *
     * \todo Currently only REP type sockets are supported. Messages must
     * currently be strings, which could be abstracted further.
     */
    template<typename HandlerIterator>
    MessageQueue(
        HandlerIterator first, HandlerIterator last, zmq::socket_t socket);

    ~MessageQueue();

    /** \brief Start handling messages
     *
     * This function blocks until termination is requested, i.e. message \ref
     * MESSAGE_TERMINATE is received. For each message it will dispatch the
     * command to the correct \ref Handler and reply with the message returned
     * by the handler. If no handler exists for the message, \ref
     * MESSAGE_ERROR is sent as reply.
     */
    void run();

private:

    using HandlerMap = std::map<std::string, Handler>;

    MessageQueue(HandlerMap handlers, zmq::socket_t socket);

    class Impl;
    std::unique_ptr<Impl> impl;
};

template<typename HandlerIterator>
MessageQueue::MessageQueue(
    HandlerIterator first, HandlerIterator last, zmq::socket_t socket) :
    MessageQueue {HandlerMap(first, last), std::move(socket)}
{
}

}
}

#endif // MESSAGING_MESSAGEQUEUE_HH_
