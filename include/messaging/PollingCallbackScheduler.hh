/** \file
 *
 * \brief Definition of Bridge::Messaging::PollingCallbackScheduler
 */

#ifndef MESSAGING_POLLINGCALLBACKSCHEDULER_HH_
#define MESSAGING_POLLINGCALLBACKSCHEDULER_HH_

#include "messaging/CallbackScheduler.hh"
#include "messaging/Sockets.hh"
#include "Thread.hh"

#include <boost/noncopyable.hpp>

#include <chrono>
#include <functional>
#include <map>

namespace Bridge {
namespace Messaging {

/** \brief Execute callbacks in MessageLoop
 *
 * PollingCallbackScheduler is an CallbackScheduler implementation that
 * integrates to MessageLoop by registering a socket that is internally use to
 * notify a PollingCallbackScheduler object of callbacks to be executed.
 *
 * PollingCallbackScheduler creates a thread in the constructor and joins it in
 * the destructor. In order to properly terminate the thread, a termination
 * notification must be sent before entering the destructor. The thread is
 * required to support delayed callbacks.
 */
class PollingCallbackScheduler :
    public CallbackScheduler, private boost::noncopyable {
public:

    /** \brief Create new callback scheduler
     *
     * \param context ZeroMQ context
     * \param terminationSubscriber Socket that will receive notification about
     * termination of the thread
     */
    PollingCallbackScheduler(
        MessageContext& context, Socket terminationSubscriber);

    /** \brief Get socket that can be registered to MessageLoop
     *
     * The socket returned by this method gets notifications about registered
     * callbacks in an internal format. The intention is that the socket is
     * registered as callback to a MessageLoop instance. The clients of the
     * class should not try to receive and interpret the messages.
     *
     * \return a socket that can be registered to MessageLoop
     */
    SharedSocket getSocket();

    /** \brief Execute callbacks
     *
     * Receive notifications about callbacks from \p socket and execute
     * them. This method is intended to be called by a MessageLoop instance,
     * using the return value from getSocket() as argument.
     *
     * The callbacks executed by the method are removed from the queue before
     * being executed. Any exceptions from the callbacks are propagated to the
     * caller. Callbacks, whether they exit by returning normally or throwing
     * exception, are only executed once.
     *
     * \param socket the socket received from getSocket() call
     */
    void operator()(Socket& socket);

private:

    void handleCallLater(
        std::chrono::milliseconds timeout, Callback callback) override;

    Socket frontSocket;
    SharedSocket backSocket;
    std::map<unsigned long, Callback> callbacks;
    Thread worker;
};

}
}

#endif // MESSAGING_POLLINGCALLBACKSCHEDULER_HH_
