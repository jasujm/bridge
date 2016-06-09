/** \file
 *
 * \brief Definition of messaging utilities
 *
 * Bridge application uses the ZeroMQ C++ bindings. While it gives RAII based
 * resource management for ZeroMQ sockets and messages, the API is still low
 * level and involves buffer management. This file defines more high level
 * messaging utilities on top of the bindings. The utilities deal with strings
 * rather than memory buffers.
 */

#ifndef MESSAGING_MESSAGEUTILITY_HH_
#define MESSAGING_MESSAGEUTILITY_HH_

#include <zmq.hpp>

#include <iterator>
#include <string>
#include <utility>

namespace Bridge {
namespace Messaging {

/** \brief Send message through socket
 *
 * \param socket socket used for sending the message
 * \param message the message to be sent (typically a string)
 * \param more true if there are more parts in the message, false otherwise
 */
template<typename MessageType>
void sendMessage(
    zmq::socket_t& socket, const MessageType& message, bool more = false)
{
    const auto flags = more ? ZMQ_SNDMORE : 0;
    socket.send(std::begin(message), std::end(message), flags);
}

/** \brief Send multipart message through socket
 *
 * Sends several messages, given as iterator range \p first and \p last, as
 * multipart message.
 *
 * \param socket socket used for sending the message
 * \param first iterator to the first message to be sent
 * \param last iterator one past the last message to be sent
 * \param more true if there are more parts in the message, false otherwise
 */
template<typename MessageIterator>
void sendMessage(
    zmq::socket_t& socket, MessageIterator first, MessageIterator last,
    bool more = false)
{
    while (first != last) {
        const auto tmp = std::next(first);
        sendMessage(socket, *first, more || tmp != last);
        first = tmp;
    }
}

/** \brief Helper for sending empty frame for router and dealer socket
 *
 * Router and dealer sockets require that message is prepended with an empty
 * frame. If \p socket is router or dealer, this function sendds the empty
 * frame (the caller of the function can then send the rest of the
 * parts). Otherwise this function does nothing.
 *
 * \note With router socket the empty frame must also be prepended by an
 * identity frame. This function does not prepend the identity frame so when
 * using router socket that must be done separately.
 *
 * \param socket the socket
 */
inline void sendEmptyFrameIfNecessary(zmq::socket_t& socket)
{
    const auto type = socket.getsockopt<zmq::socket_type>(ZMQ_TYPE);
    if (type == zmq::socket_type::router || type == zmq::socket_type::dealer) {
        sendMessage(socket, std::string {}, true);
    }
}

/** \brief Receive message sent through socket
 *
 * \tparam String the type of the string returned by the function
 *
 * \param socket the socket used to receive the message
 *
 * \return A pair containing the following
 *   - string containing the message
 *   - boolean indicating whether there are more parts in the message
 */
template<typename String = std::string>
std::pair<String, bool> recvMessage(zmq::socket_t& socket)
{
    using CharType = typename String::value_type;
    auto msg = zmq::message_t {};
    socket.recv(&msg);
    const auto more = socket.getsockopt<std::int64_t>(ZMQ_RCVMORE);
    return {{msg.data<CharType>(), msg.size() / sizeof(CharType)}, more != 0};
}

/** \brief Receive (and ignore) empty frame if necessary
 *
 * If \p socket type is router or dealer, one frame is received and
 * ignore. Otherwise the call does nothing. This function can be used to
 * skip the initial empty frame for dealer and route sockets that maintain
 * compatibility with req and rep sockets.
 *
 * \param socket
 *
 * \todo The first part is currently just ignored. It could be verified that
 * it is empty frame.
 */
inline bool recvEmptyFrameIfNecessary(zmq::socket_t& socket)
{
    const auto type = socket.getsockopt<zmq::socket_type>(ZMQ_TYPE);
    auto more = true;
    if (type == zmq::socket_type::router || type == zmq::socket_type::dealer) {
        const auto message = recvMessage(socket);
        more = message.second;
    }
    return more;
}

/** \brief Receive all parts of a multipart message
 *
 * If \p socket is router or dealer, the first part is ignored. For router
 * socket this is expected to be called after extracting the identity frame by
 * other means.
 *
 * \tparam String the type of the string written to the output iterator
 *
 * \param out output iterator the messages are written to
 * \param socket socket used to receive the messages
 */
template<typename String = std::string, typename OutputIterator>
void recvAll(OutputIterator out, zmq::socket_t& socket)
{
    auto more = recvEmptyFrameIfNecessary(socket);
    while (more) {
        auto message = recvMessage<String>(socket);
        *out++ = std::move(message.first);
        more = message.second;
    }
}

}
}

#endif // MESSAGING_MESSAGEUTILITY_HH_
