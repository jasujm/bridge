/** \file
 *
 * \brief Definition of Bridge::Messaging::MessageQueue class
 */

#ifndef MESSAGING_MESSAGEQUEUE_HH_
#define MESSAGING_MESSAGEQUEUE_HH_

#include <zmq.hpp>

#include <map>
#include <memory>
#include <utility>

#include "Blob.hh"
#include "BlobMap.hh"

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
 * message to correct message handler and replying. The MessageQueue object does
 * not own the sockets used to receive and send the messages. Instead
 * MessageQueue is designed to be used with MessageLoop that handles the actual
 * polling of the sockets.
 *
 * MessageQueue uses request–reply pattern (although the sockets used do not
 * have to be rep sockets). The messages sent to the message queue are commands
 * with arbitrary number of arguments to be handled by the handler for the
 * command. A recognized command (i.e. command for which a handler is
 * registered) causes the handler for that command to be executed. Based on the
 * result of the handling either success or failure is reported.
 *
 * MessageQueue does not interpret commands or arguments in any way, but handles
 * them as arbitrary byte sequences. Matching the command is done by binary
 * comparison.
 */
class MessageQueue {
public:

    /** \brief Map from commands to message handlers
     *
     * HandlerMap is mapping between commands (strings) and MessageHandler
     * objects used to handle the messages.
     */
    using HandlerMap = BlobMap<std::shared_ptr<MessageHandler>>;

    /** \brief Create message queue
     *
     * \param handlers mapping from commands to message handlers
     */
    explicit MessageQueue(HandlerMap handlers);

    ~MessageQueue();

    /** \brief Try to set handler for a command
     *
     * If there is no handler for \p command, or \p handler points to the same
     * MessageHandler that is already assigned for \p command, the handler is
     * assigned and the call returns true. Otherwise the call has no effect and
     * returns false.
     *
     * \return true if successful, false otherwise
     */
    bool trySetHandler(
        ByteSpan command, std::shared_ptr<MessageHandler> handler);

    /** \brief Receive and reply the next message
     *
     * When called, the method receives a message from \p socket, dispatches
     * it to the correct handler and sends the reply through \p socket. The
     * reply is either REPLY_SUCCESS or REPLY_FAILURE, as determined by the
     * MessageHandler the message is dispatched to. If the MessageHandler
     * generates output, the output is interpreted as the arguments of the
     * reply, and sent after the initial REPLY_SUCCESS or REPLY_FAILURE frame
     * in multipart message, each entry in its own frame. If handling the
     * message fails or there is no handler, REPLY_FAILURE is sent to the
     * socket.
     *
     * \note The intention is that MessageQueue acts as a “server” receiving
     * messages and replying back to the “client” who sent the message. The
     * type of \p socket should support the intended use. For example rep,
     * pair or router are such socket types.
     *
     * \note MessageQueue passes the identity received from the socket as
     * argument to the MessageHandler instances. If \p socket is not router, the
     * Identity::routingId is always an empty blob.
     *
     * \param socket the socket the message is received from and the reply is
     * sent to
     */
    void operator()(zmq::socket_t& socket);

private:

    HandlerMap handlers;
};

}
}

#endif // MESSAGING_MESSAGEQUEUE_HH_
