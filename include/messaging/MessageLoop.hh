/** \file
 *
 * \brief Definition of Bridge::Messaging::MessageLoop class
 */

#ifndef MESSAGING_MESSAGELOOP_HH_
#define MESSAGING_MESSAGELOOP_HH_

#include "messaging/Poller.hh"
#include "messaging/Sockets.hh"

#include <boost/noncopyable.hpp>

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
     * The message loop is initially empty
     *
     * Upon creation the message loop blocks SIGINT and SIGTERM. The loop takes
     * responsibility of handling those signals by doing cleanup and finally
     * exiting run() method. Signal masks are cleaned when the loop is
     * destructed.
     *
     * Any worker thread should be created after the message loop is created in
     * the main thread in order to ensure that the signal mask is also applied
     * to them.
     *
     * \param context The ZeroMQ context used by sockets registered to the
     * message loop
     */
    MessageLoop(MessageContext& context);

    ~MessageLoop();

    /** \brief Start polling messages
     *
     * Call to this method starts the message loop.
     *
     * The message loop will swallow any exceptions from the callbacks, and log
     * them as errors.
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
    Socket createTerminationSubscriber();

private:

    void handleAddPollable(
        PollableSocket socket, SocketCallback callback) override;

    void handleRemovePollable(Socket& socket) override;

    class Impl;
    const std::unique_ptr<Impl> impl;
};


}
}

#endif // MESSAGING_MESSAGELOOP_HH_
