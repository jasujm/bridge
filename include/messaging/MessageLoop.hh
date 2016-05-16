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
     * \sa addSocket(), run()
     */
    using Callback = std::function<bool(zmq::socket_t&)>;

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
     * \param socket the socket to add to the loop
     */
    void addSocket(zmq::socket_t& socket, Callback callback);

    /** \brief Start polling messages
     *
     * Call to this method starts the message loop which consists of polling
     * incoming messages, and calling callbacks for the sockets that can
     * receive messages. Thus when callback is called, the socket given as its
     * argument is guaranteed to not block when message is received from it.
     *
     * The method block until a callback return false.
     *
     * The method catches zmq::error_t indicating interruption, which causes
     * the method to terminate cleanly. All other exceptions are propagated as
     * is.
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
