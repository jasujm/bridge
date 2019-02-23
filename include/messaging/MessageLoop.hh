/** \file
 *
 * \brief Definition of Bridge::Messaging::MessageLoop class
 */

#ifndef MESSAGING_MESSAGELOOP_HH_
#define MESSAGING_MESSAGELOOP_HH_

#include "messaging/Poller.hh"

#include <boost/noncopyable.hpp>
#include <zmq.hpp>

namespace Bridge {
namespace Messaging {

/** \brief Event loop based on polling several ZeroMQ sockets
 *
 * MessageLoop is an event loop whose events consist of ZeroMQ messages. The
 * application utilizing MessageLoop registers several sockets that are
 * polled. The application is then signaled by calling a callback. The sockets
 * are shared if the client of the class wishes to retain ownership.
 *
 * Sockets and callbacks are manager through Poller::addPollable() and
 * Poller::removePollable() interface. Polling starts when calling run().
 *
 * In addition to handling incoming ZeroMQ messages, MessageLoop handles SIGTERM
 * and SIGINT signals by terminating cleanly.
 */
class MessageLoop : public Poller, private boost::noncopyable {
public:

    /** \brief Create new message loop
     *
     * The message loop is initially empty.
     *
     * \param context The ZeroMQ context used by sockets registered to the
     * message loop
     */
    MessageLoop(zmq::context_t& context);

    ~MessageLoop();

    /** \brief Start polling messages
     *
     * Call to this method starts the message loop.
     *
     * Before entering the message loop this method blocks SIGINT and
     * SIGTERM. MessageLoop takes responsibility of handling those signals by
     * doing cleanup and finally exiting run() method. Signal masks are cleaned
     * when the method is exited, whether by exception or otherwise.
     *
     * \exception The method catches zmq::error_t indicating interruption. All
     * other exceptions are propagated as is.
     */
    void run();

    /** \brief Create socket that is notified when the message loop terminates
     *
     * This method creates and returns a SUB socket. It is automatically
     * subscribed to an internal PUB socket that will send a single empty frame
     * when the message loop exists. Both the internal PUB socket and the
     * created SUB socket share the context passed to the MessageLoop()
     * constructor.
     *
     * The intention is to use this socket to notify other threads when they
     * need to exit.
     *
     * \return Socket that will be notified when the message loop exits
     */
    zmq::socket_t createTerminationSubscriber();

private:

    void handleAddPollable(
        PollableSocket socket, SocketCallback callback) override;

    void handleRemovePollable(zmq::socket_t& socket) override;

    class Impl;
    const std::unique_ptr<Impl> impl;
};


}
}

#endif // MESSAGING_MESSAGELOOP_HH_
