/** \file
 *
 * \brief Definition of Bridge::Messaging::MessageQueue class
 */

#ifndef MESSAGING_MESSAGEQUEUE_HH_
#define MESSAGING_MESSAGEQUEUE_HH_

#include <boost/noncopyable.hpp>
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
     * \param endpoint the ZeroMQ endpoint the message queue binds to
     * \param terminateCommand If provided, command for requesting termination
     * of the message queue. If not provided, the message queue will have no
     * termination command.
     */
    MessageQueue(
        HandlerMap handlers, zmq::context_t& context,
        const std::string& endpoint,
        const boost::optional<std::string>& terminateCommand = boost::none);

    ~MessageQueue();

    /** \brief Start handling messages
     *
     * This function blocks until termination is requested using
     * terminate(). For each message it will dispatch the command to the
     * correct MessageHandler. If the handling is successful, a multipart
     * message containing REPLY_SUCCESS as its first part and strings returned
     * by the handler as subsecuent parts is sent. If the handling fails or no
     * handler is registered for the message, a message containing
     * REPLY_FAILURE is sent.
     *
     * This method is exception neutral with regards to all exceptions expect
     * zmq::error_t indicating interrupt when receiving messages. If
     * interrupted, the exception is caught and message handling terminated
     * cleanly.
     */
    void run();

    /** \brief Request the message queue to terminate
     *
     * After this function is called (from kernel signal handler, message
     * handler etc.), run() will return. If run() is called after this
     * function, it will return immediately.
     *
     * \note It is safe to call this function from other threads or signal
     * handlers. However, for inter‐thread use, termination by message is
     * preferred.
     */
    void terminate();

private:

    class Impl;
    const std::unique_ptr<Impl> impl;
};

}
}

#endif // MESSAGING_MESSAGEQUEUE_HH_
