/** \file
 *
 * \brief Definition of Bridge::Main::CallbackScheduler
 */

#include <boost/noncopyable.hpp>
#include <zmq.hpp>

#include <functional>
#include <memory>
#include <queue>

namespace Bridge {
namespace Main {

/** \brief Execute callbacks in MessageLoop
 *
 * CallbackScheduler can be used to execute callbacks asynchronously, outside of
 * the caller’s stack frame. It integrates to Messaging::MessageLoop by
 * registering a socket that is internally use to notify a CallbackScheduler
 * object of callbacks to be executed.
 */
class CallbackScheduler : private boost::noncopyable {
public:

    /** \brief Callback function
     */
    using Callback = std::function<void()>;

    /** \brief Create new callback scheduler
     *
     * \param context ZeroMQ context
     */
    CallbackScheduler(zmq::context_t& context);

    /** \brief Schedule new callback
     *
     * This function is used to schedule a function to be executed. The
     * callbacks are executed when \ref operator()() is called, usually by a
     * Messaging::MessageLoop instance.
     *
     * The client is responsible for ensuring that any objects captured by
     * reference in the \p callback have lifetime exceeding that of the callback
     * function.
     *
     * \param callback the callback to be registered
     */
    void callOnce(Callback callback);

    /** \brief Get socket that can be registered to Messaging::MessageLoop
     *
     * The socket returned by this method gets notifications about registered
     * callbacks in an internal format. The intention is that the socket is
     * registered as callback to a Messaging::MessageLoop instance. The clients
     * of the class should not try to receive and interpret the messages.
     *
     * \return a socket that can be registered to Messaging::MessageLoop
     */
    std::shared_ptr<zmq::socket_t> getSocket();

    /** \brief Execute callbacks
     *
     * Receive notifications about callbacks from \p socket and execute
     * them. This method is intended to be called by a Messaging::MessageLoop
     * instance, using the return value from getSocket() as argument.
     *
     * The callbacks executed by the method are removed from the queue before
     * being executed. Any exceptions from the callbacks are propagated to the
     * caller. Callbacks, whether they exit by returning normally or throwing
     * exception, are only executed once.
     *
     * \param socket the socket received from getSocket() call
     */
    void operator()(zmq::socket_t& socket);

private:

    zmq::socket_t frontSocket;
    std::shared_ptr<zmq::socket_t> backSocket;
    std::queue<Callback> callbacks;
};

}
}
