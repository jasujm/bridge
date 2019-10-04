/** \file
 *
 * The messaging framework uses cppzmq (https://github.com/zeromq/cppzmq) as C++
 * interface around the ZeroMQ library. This header contains thin wrappers
 * around the library types.
 *
 * \brief Bridge messaging framework socket definitions
 */

#ifndef MESSAGING_SOCKETS_HH_
#define MESSAGING_SOCKETS_HH_

#include <iterator>
#include <memory>
#include <stdexcept>
#include <string>

#include <zmq.hpp>

#include "Blob.hh"

namespace Bridge {
namespace Messaging {

/** \brief ZeroMQ context type
 */
using MessageContext = zmq::context_t;

/** \brief ZeroMQ socket type
 */
using Socket = zmq::socket_t;

/** \brief Enumeration containing types for ZeroMQ sockets
 */
using SocketType = zmq::socket_type;

/** \brief ZeroMQ socket type with shared ownership
 */
using SharedSocket = std::shared_ptr<Socket>;

/** \brief ZeroMQ message type
 */
using Message = zmq::message_t;

/** \brief Flags passed to Socket::send
 */
using SendFlags = zmq::send_flags;

/** \brief Flags passed to Socket::send
 */
using RecvFlags = zmq::recv_flags;

/** \brief ZeroMQ related exception
 */
using SocketError = zmq::error_t;

/** \brief ZeroMQ socket pollitem
 */
using Pollitem = zmq::pollitem_t;

/** \brief Wrapper over \c zmq::buffer
 *
 * \param args the arguments to \c zmq::buffer
 *
 * \return the result of \c zmq::buffer called with \p args
 */
template<typename... Args>
constexpr auto messageBuffer(Args&&... args)
{
    return zmq::buffer(std::forward<Args>(args)...);
}

/** \brief Construct \c zmq::const_buffer object from byte span
 *
 * \param bytes The bytes to wrap into the buffer
 *
 * \return \c zmq::const_buffer object containing \p bytes
 */
constexpr auto messageBuffer(ByteSpan bytes)
{
    return zmq::const_buffer(bytes.data(), bytes.size());
}

/** \brief Wrapper over \c std::make_shared<Socket>
 *
 * \param args the arguments to \c std::make_shared
 *
 * \return SharedSocket constructed from the arguments
 */
template<typename... Args>
auto makeSharedSocket(Args&&... args)
{
    return std::make_shared<Socket>(std::forward<Args>(args)...);
}

/** \brief Wrapper over \c Socket::bind
 *
 * \param socket the socket to bind
 * \param endpoint the endpoint
 */
inline void bindSocket(Socket& socket, std::string_view endpoint)
{
    socket.bind(endpoint.data());
}

/** \brief Wrapper over \c Socket::unbind
 *
 * \param socket the socket to unbind
 * \param endpoint the endpoint
 */
inline void unbindSocket(Socket& socket, std::string_view endpoint)
{
    socket.unbind(endpoint.data());
}

/** \brief Wrapper over \c Socket::connect
 *
 * \param socket the socket to connect
 * \param endpoint the endpoint
 */
inline void connectSocket(Socket& socket, std::string_view endpoint)
{
    socket.connect(endpoint.data());
}

/** \brief Wrapper over \c Socket::disconnect
 *
 * \param socket the socket to disconnect
 * \param endpoint the endpoint
 */
inline void disconnectSocket(Socket& socket, std::string_view endpoint)
{
    socket.disconnect(endpoint.data());
}

/** \brief Wrapper over \c zmq::poll
 *
 * \param pollitems list of Pollitem objects
 * \param timeout the timeout
 *
 * \return the result of \c zmq::poll called with the arguments (i.e. the number
 * of ready sockets)
 */
template<typename PollitemRange, typename Timeout = long>
auto pollSockets(PollitemRange& pollitems, Timeout timeout = -1)
{
    return zmq::poll(std::data(pollitems), std::size(pollitems), timeout);
}

/** \brief Wrapper over \c Socket::send
 *
 * This function sends \p message using blocking I/O, so it is assumed that the
 * send operation never fails with EAGAIN.
 *
 * \param socket the socket used to send \p message
 * \param message a message‐like object (Message object or buffer)
 * \param more if true, the message is sent with ZMQ_SNDMORE flag
 */
template<typename MessageLike>
void sendMessage(Socket& socket, MessageLike&& message, bool more = false)
{
    const auto flags = more ? SendFlags::sndmore : SendFlags::none;
    const auto success = socket.send(std::forward<MessageLike>(message), flags);
    if (!success) {
        throw std::runtime_error {
            "Blocking send unexpectedly failed with EAGAIN"};
    }
}

/** \brief Wrapper over \c Socket::send
 *
 * This function sends \p message using non‐blocking I/O, so it is assumed that
 * the send operation may fail with EAGAIN.
 *
 * \param socket the socket used to send \p message
 * \param message a message‐like object (Message object or buffer)
 * \param more if true, the message is sent with ZMQ_SNDMORE flag

 * \return true if \p socket was ready and the message could be sent, or false
 * otherwise
 */
template<typename MessageLike>
[[nodiscard]] bool sendMessageNonblocking(
    Socket& socket, MessageLike&& message, bool more = false)
{
    const auto flags = more ? SendFlags::sndmore : SendFlags::none;
    const auto success = socket.send(
        std::forward<MessageLike>(message), flags | SendFlags::dontwait);
    return bool(success);
}

/** \brief Wrapper over \c Socket::recv
 *
 * This function receives \p message using blocking I/O, so it is assumed that
 * the receive operation never fails with EAGAIN.
 *
 * \param socket the socket used to receive \p message
 * \param message a message‐like object (Message object or buffer)
 *
 * \return the result of \c zmq::socket_t::recv called with the arguments
 * (i.e. the number of bytes received for Message object, or number of actual
 * and expected bytes for a buffer). The \c optional object is automatically
 * unwrapped because blocking receive always returns a non‐empty result.
 */
template<typename MessageLike>
auto recvMessage(Socket& socket, MessageLike&& message)
{
    const auto result = socket.recv(
        std::forward<MessageLike>(message), RecvFlags::none);
    if (!result) {
        throw std::runtime_error {
            "Blocking recv unexpectedly failed with EAGAIN"};
    }
    return *result;
}

/** \brief Wrapper over \c Socket::recv
 *
 * This function receives \p message using non‐blocking I/O, so it is assumed
 * that the send operation may fail with EAGAIN.
 *
 * \param socket the socket used to receive \p message
 * \param message a message‐like object (Message object or buffer)
 *
 * \return the result of \c zmq::socket_t::recv called with the arguments
 * (i.e. the number of bytes received for Message object, or number of actual
 * and expected bytes for a buffer).
 */
template<typename MessageLike>
[[nodiscard]] auto recvMessageNonblocking(Socket& socket, MessageLike&& message)
{
    const auto result = socket.recv(
        std::forward<MessageLike>(message), RecvFlags::dontwait);
    return result;
}

}
}

#endif // MESSAGING_SOCKETS_HH_
