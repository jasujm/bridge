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

/** \brief Loop polling several ZeroMQ sockets
 *
 * MessageLoop class can be used as the event loop for an application or
 * thread that is based on polling several ZeroMQ sockets.
 */
class MessageLoop : private boost::noncopyable {
public:

    /** \brief Callback for handling receiving message from the socket
     *
     * \sa addSocket()
     */
    using SocketCallback = std::function<bool(zmq::socket_t&)>;

    /** \brief Callback that can be registered to be called by the message loop
     *
     * \sa callOnce()
     */
    using SimpleCallback = std::function<void()>;

    /** \brief Create new message loop
     *
     * The message loop is initially empty. Sockets can be tied to callbacks
     * using addSocket().
     */
    MessageLoop();

    ~MessageLoop();

    /** \brief Add socket to the message loop
     *
     * Add new socket to the message loop and associate a callback to handle
     * messages sent to the socket. A callback is a function that receives the
     * socket as its sole parameter. The callback should return true if the
     * message loop should continue and false if it should terminate.
     *
     * \note The \p socket is accepted as shared pointer and stored for the
     * whole lifetime of the loop. It is the responsibility of the client to
     * ensure that \p callback also remains valid during the lifetime of the
     * loop.
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
     * The method block until a callback return false.
     *
     * The method catches zmq::error_t indicating interruption, and interrupts
     * it. All other exceptions are propagated as is. It is intended that an
     * interruption handler wishing to terminate the program calls terminate().
     */
    void run();

    /** \brief Request the message loop to terminate
     *
     * After this function is called (from kernel signal handler, message
     * handler etc.), run() will return. If run() is called after this
     * function, it will return immediately.
     *
     * \note It is safe to call this function from other threads or signal
     * handlers. However, for inter‐thread use, termination by returning false
     * from callback is preferred.
     */
    void terminate();

private:

    class Impl;
    const std::unique_ptr<Impl> impl;
};


}
}

#endif // MESSAGING_MESSAGELOOP_HH_
