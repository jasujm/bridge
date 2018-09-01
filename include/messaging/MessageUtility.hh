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

#include <boost/endian/conversion.hpp>
#include <zmq.hpp>

#include <iterator>
#include <string>
#include <type_traits>
#include <utility>

#include "Blob.hh"

namespace Bridge {
namespace Messaging {

/** \brief Send message through socket
 *
 * \tparam MessageType the type of the message. It must be a standard container
 * (vector, string etc.) holding scalar values.
 *
 * \param socket socket used for sending the message
 * \param message the message to be sent (typically a string)
 * \param more true if there are more parts in the message, false otherwise
 */
template<typename MessageType>
void sendMessage(
    zmq::socket_t& socket, const MessageType& message, bool more = false)
{
    static_assert(
        std::is_scalar_v<
            typename std::iterator_traits<
                decltype(std::begin(message))>::value_type>,
        "Message type must contain scalar values");
    const auto flags = more ? ZMQ_SNDMORE : 0;
    socket.send(std::begin(message), std::end(message), flags);
}

/** \brief Send multipart message through socket
 *
 * Sends several messages, given as iterator range \p first and \p last, as
 * multipart message.
 *
 * \tparam MessageType the type of the message. It must be a standard container
 * (vector, string etc.) holding scalar values.
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

/** \brief Send binary value
 *
 * This function can be used to send binary representation of a value directly
 * to the wire. The value is not serialized but rather the bytes are converted
 * to big endian order and sent as is.
 *
 * \tparam EndianReversible see
 * http://www.boost.org/doc/libs/1_63_0/libs/endian/doc/conversion.html#EndianReversible
 *
 * \param socket socket used for sending the message
 * \param v the value to send
 * \param more true if there are more parts in the message, false otherwise
 */
template<typename EndianReversible>
void sendValue(
    zmq::socket_t& socket, const EndianReversible& v, bool more = false)
{
    const auto big_endian_v = boost::endian::native_to_big(v);
    const auto flags = more ? ZMQ_SNDMORE : 0;
    socket.send(std::addressof(big_endian_v), sizeof(big_endian_v), flags);
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
 * \tparam MessageType the type of the message. It must be a standard container
 * (vector, string etc.) holding scalar values.
 *
 * \param socket the socket used to receive the message
 *
 * \return A pair containing the following
 *   - the message (single frame)
 *   - boolean indicating whether there are more parts in the message
 */
template<typename MessageType = std::string>
std::pair<MessageType, bool> recvMessage(zmq::socket_t& socket)
{
    static_assert(
        std::is_scalar_v<typename MessageType::value_type>,
        "Message type must contain scalar values");
    using ValueType = typename MessageType::value_type;
    auto msg = zmq::message_t {};
    socket.recv(&msg);
    const auto first = msg.data<ValueType>();
    const auto last = first + msg.size() / sizeof(ValueType);
    const auto more = socket.getsockopt<std::int64_t>(ZMQ_RCVMORE);
    return {MessageType(first, last), more != 0};
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
 * \tparam MessageType the type of the message. It must be a standard container
 * (vector, string etc.) holding scalar values.
 *
 * \param out output iterator the messages are written to
 * \param socket socket used to receive the messages
 *
 * \deprecated Do not use for router sockets. More robust identity handling
 * implemented in the MessageQueue class which the only class in the messaging
 * framework currently properly supporting router sockets.
 */
template<typename MessageType = std::string, typename OutputIterator>
void recvAll(OutputIterator out, zmq::socket_t& socket)
{
    auto more = recvEmptyFrameIfNecessary(socket);
    while (more) {
        auto message = recvMessage<MessageType>(socket);
        *out++ = std::move(message.first);
        more = message.second;
    }
}

/** \brief Get view to the bytes \p message consists of
 *
 * \param message the message
 *
 * \return ByteSpan object over the bytes \p message consists of
 */
inline ByteSpan messageView(zmq::message_t& message)
{
    const auto* data = message.data<ByteSpan::value_type>();
    const auto size = static_cast<ByteSpan::index_type>(
        message.size() * sizeof(*data));
    return {data, size};
}

}
}

#endif // MESSAGING_MESSAGEUTILITY_HH_
