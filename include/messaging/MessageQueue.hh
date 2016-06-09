/** \file
 *
 * \brief Definition of Bridge::Messaging::MessageQueue class
 */

#ifndef MESSAGING_MESSAGEQUEUE_HH_
#define MESSAGING_MESSAGEQUEUE_HH_

#include <boost/optional/optional.hpp>
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
 * MessageQueue is an object receiving messages from sockets, dispatching the
 * message to correct message handler and replying. The MessageQueue object
 * does not own the sockets used to receive and send the messages. Rather
 * MessageQueue is designed to be used with MessageLoop that handles the
 * actual polling of the sockets.
 *
 * MessageQueue uses request–reply pattern. The messages sent to the message
 * queue are strings that represent commands. A recognized command
 * (i.e. command for which a handler is registered) causes the handler for
 * that command to be executed. Based on the result of the handling either
 * success or failure is reported.
 */
class MessageQueue {
public:

    /** \brief Map from commands to message handlers
     *
     * HandlerMap is mapping between commands (strings) and MessageHandler
     * objects used to handle the messages.
     */
    using HandlerMap = std::map<std::string, std::shared_ptr<MessageHandler>>;

    /** \brief Create message queue
     *
     * \param handlers mapping from commands to message handlers
     * \param terminateCommand If provided, command for requesting termination
     * of the message queue. If not provided, the message queue will have no
     * termination command.
     */
    explicit MessageQueue(
        HandlerMap handlers,
        boost::optional<std::string> terminateCommand = boost::none);

    ~MessageQueue();

    /** \brief Receive and reply the next message
     *
     * When called, the method receives a message from \p socket, dispatch it
     * to the correct handler and sends the reply through \p socket. The reply
     * is either REPLY_SUCCESS or REPLY_FAILURE, as determined by the
     * MessageHandler the message is dispatched to. If the MessageHandler has
     * output, the output follows the initial REPLY_SUCCESS or REPLY_FAILURE
     * frame in multipart message, each entry in its own frame. If handling
     * the message fails or there is no handler, REPLY_FAILURE is sent to the
     * socket.
     *
     * \note If \p socket is not router, the identity passed to the message
     * handler is always an empty string.
     *
     * \param socket the socket the message is received from and the reply is
     * sent to
     *
     * \return true if the command was not termination, false otherwise
     */
    bool operator()(zmq::socket_t& socket);

private:

    HandlerMap handlers;
    boost::optional<std::string> terminateCommand;
};

}
}

#endif // MESSAGING_MESSAGEQUEUE_HH_
