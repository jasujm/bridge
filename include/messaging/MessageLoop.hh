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
 * In addition to handling incoming ZeroMQ messages, MessageLoop handles SIGTERM
 * and SIGINT signals by terminating cleanly.
 */
class MessageLoop : private boost::noncopyable {
public:

    /** \brief Callback for handling receiving message from a socket
     *
     * \sa addSocket()
     */
    using SocketCallback = std::function<void(zmq::socket_t&)>;

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

    /** \brief Start polling messages
     *
     * Call to this method starts the message loop which consists of polling
     * incoming messages, and calling callbacks for the sockets that can
     * receive messages. Thus when callback is called, the socket given as its
     * argument is guaranteed to not block when message is received from it.
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

private:

    class Impl;
    const std::unique_ptr<Impl> impl;
};


}
}

#endif // MESSAGING_MESSAGELOOP_HH_
