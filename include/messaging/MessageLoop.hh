/** \file
 *
 * \brief Definition of Bridge::Messaging::MessageLoop class
 */

#ifndef MESSAGING_MESSAGELOOP_HH_
#define MESSAGING_MESSAGELOOP_HH_

#include <boost/noncopyable.hpp>
#include <zmq.hpp>

#include <functional>
#include <memory>
#include <vector>

namespace Bridge {
namespace Messaging {

/** \brief Event loop based on polling several ZeroMQ sockets
 *
 * MessageLoop is an event loop whose events consist of ZeroMQ messages. The
 * application utilizing MessageLoop registers several sockets that are
 * polled. The application is then signaled by calling a callback. The sockets
 * are shared if the client of the class wishes to retain ownership.
 *
 * In addition MessageLoop supports immediate callbacks that are dispatched
 * sequentially. They can be used to defer user code to be called once a
 * previous callback has returned.
 */
class MessageLoop : private boost::noncopyable {
public:

    /** \brief Callback for handling receiving message from a socket
     *
     * \sa addSocket()
     */
    using SocketCallback = std::function<void(zmq::socket_t&)>;

    /** \brief Callback that can be registered to be called once by the
     * message loop
     *
     * \sa callOnce()
     */
    using SimpleCallback = std::function<void()>;

    /** \brief Create new message loop
     *
     * The message loop is initially empty.
     */
    MessageLoop();

    ~MessageLoop();

    /** \brief Register socket to the message loop
     *
     * Add new socket to the message loop and associate a callback to handle
     * messages received from the socket. The invocation of the callbacks
     * starts when run() is called.
     *
     * \note The \p socket is accepted as shared pointer and stored for the
     * whole lifetime of the loop. It is the responsibility of the client to
     * ensure that the lifetime of any object captured by reference by \p
     * callback exceeds the lifetime of the MessageLoop.
     *
     * \param socket the socket to add to the loop
     * \param callback The function called when socket is ready to receive
     * message. The parameter to the callback is reference to \p socket.
     */
    void addSocket(
        std::shared_ptr<zmq::socket_t> socket, SocketCallback callback);

    /** \brief Enqueue a simple callback
     *
     * The callbacks in the queue are called in the registration order after
     * the socket handler or another callback returns. Callbacks are dequeued
     * once called. This method can be used to ensure that an action is taken
     * once the previous handler has completely finished.
     *
     * If \p callback captures objects by reference, it is the responsibility
     * of the client to ensure that the lifetime of any object captured by \p
     * callback exceeds the lifetime of the MessageLoop.
     *
     * \param callback the callback
     */
    void callOnce(SimpleCallback callback);

    /** \brief Start polling messages
     *
     * Call to this method starts the message loop which consists of polling
     * incoming messages, and calling callbacks for the sockets that can
     * receive messages. Thus when callback is called, the socket given as its
     * argument is guaranteed to not block when message is received from it.
     *
     * The method catches zmq::error_t indicating interruption. All other
     * exceptions are propagated as is. Note that interruption does not
     * automatically terminate the loop. It is intended that an interruption
     * handler wishing to terminate the loop calls terminate().
     */
    void run();

    /** \brief Request the message loop to terminate
     *
     * After this function is called (from kernel signal handler, message
     * handler etc.), run() will return. If run() is called after this
     * function, it will return immediately.
     *
     * \note It is safe to call this function from other threads or signal
     * handlers.
     */
    void terminate();

private:

    class Impl;
    const std::unique_ptr<Impl> impl;
};


}
}

#endif // MESSAGING_MESSAGELOOP_HH_
