/** \file
 *
 * \brief Definition of Bridge::Messaging::MessageQueue class
 */

#ifndef MESSAGING_MESSAGEQUEUE_HH_
#define MESSAGING_MESSAGEQUEUE_HH_

#include <boost/noncopyable.hpp>
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

class MessageHandler;

/** \brief Message queue for communicating between threads and processes
 *
 * Bridge application threads are built around exchanging information and
 * requests through messages. MessageQueue is responsible for receiving and
 * dispatching the messages to handlers.
 *
 * MessageQueue uses request–reply pattern. The messages sent to the message
 * queue are strings that represent commands. A recognized command
 * (i.e. command for which a handler is registered) causes the handler for
 * that command to be executed. Based on the result of the handling either
 * success or failure is reported.
 */
class MessageQueue : private boost::noncopyable {
public:

    /** \brief Map from commands to message handlers
     *
     * HandlerMap is mapping between commands (strings) and MessageHandler
     * objects used to handle the messages.
     */
    using HandlerMap = std::map<std::string, std::shared_ptr<MessageHandler>>;

    /** \brief Message used to request termination
     */
    static const std::string MESSAGE_TERMINATE;

    /** \brief Message used as reply to indicate successful request
     */
    static const std::string REPLY_SUCCESS;

    /** \brief Message used as reply to indicate failure to fulfill the request
     */
    static const std::string REPLY_FAILURE;

    /** \brief Create message queue
     *
     * \param handlers mapping from commands to message handlers
     * \param context the ZeroMQ context for the message queue
     * \param address the ZeroMQ address the message queue binds to
     */
    MessageQueue(
        HandlerMap handlers, zmq::context_t& context,
        const std::string& address);

    /** \brief Create message queue
     *
     * \tparam HandlerIterator Iterator that, when dereferenced, returns a
     * pair containing the message and a std::shared_ptr to MessageHandler to
     * handle that message.
     *
     * \param first iterator to the first handler
     * \param last iterator to the last handler
     * \param context the ZeroMQ context for the message queue
     * \param address the ZeroMQ address the message queue binds to
     */
    template<typename HandlerIterator>
    MessageQueue(
        HandlerIterator first, HandlerIterator last,
        zmq::context_t& context, const std::string& address);

    ~MessageQueue();

    /** \brief Start handling messages
     *
     * This function blocks until termination is requested, i.e. message \ref
     * MESSAGE_TERMINATE is received. For each message it will dispatch the
     * command to the correct MessageHandler. If the handling is successful, a
     * multipart message containing REPLY_SUCCESS as its first part and
     * strings returned by the handler as subsecuent parts is sent. If the
     * handling fails or no handler is registered for the message, a message
     * containing REPLY_FAILURE is sent.
     */
    void run();

private:

    class Impl;
    const std::unique_ptr<Impl> impl;
};

template<typename HandlerIterator>
MessageQueue::MessageQueue(
    HandlerIterator first, HandlerIterator last,
    zmq::context_t& context, const std::string& address) :
    MessageQueue {HandlerMap(first, last), context, address}
{
}

}
}

#endif // MESSAGING_MESSAGEQUEUE_HH_
