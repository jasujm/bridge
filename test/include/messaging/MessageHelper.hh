/** \file
 *
 * \brief Utilities that used to be in messaging/MessageUtility.hh
 *
 * These utilities were once used in production code but have since been
 * deprecated. Only in use in unit tests.
 */

#ifndef MESSAGING_MESSAGEHELPER_HH_
#define MESSAGING_MESSAGEHELPER_HH_

#include "messaging/MessageUtility.hh"

#include <string>
#include <utility>

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
 *
 * \deprecated Use messageFromValue() or messageFromContainer() with the ZMQ API
 * for more flexibility.
 */
template<typename MessageType>
void sendMessage(
    Messaging::Socket& socket, const MessageType& message, bool more = false)
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
 * (blob, vector, string etc.) holding scalar values.
 *
 * \param socket socket used for sending the message
 * \param first iterator to the first message to be sent
 * \param last iterator one past the last message to be sent
 * \param more true if there are more parts in the message, false otherwise
 *
 * \deprecated Use messageFromValue() or messageFromContainer() with
 * sendMultipart() for more flexibility.
 */
template<typename MessageIterator>
void sendMessage(
    Messaging::Socket& socket, MessageIterator first, MessageIterator last,
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
 *
 * \deprecated Use messageFromValue() with the ZMQ API for more flexibility.
 */
template<typename EndianReversible>
void sendValue(
    Messaging::Socket& socket, const EndianReversible& v, bool more = false)
{
    const auto big_endian_v = boost::endian::native_to_big(v);
    const auto flags = more ? ZMQ_SNDMORE : 0;
    socket.send(std::addressof(big_endian_v), sizeof(big_endian_v), flags);
}

/** \brief Receive message sent through socket
 *
 * \tparam MessageType the type of the message. It must be a standard container
 * (blob, vector, string etc.) holding scalar values.
 *
 * \param socket the socket used to receive the message
 *
 * \return A pair containing the following
 *   - the message (single frame)
 *   - boolean indicating whether there are more parts in the message
 *
 * \deprecated Use ZMQ API with serializers for more flexibility.
 */
template<typename MessageType>
std::pair<MessageType, bool> recvMessage(Messaging::Socket& socket)
{
    static_assert(
        std::is_scalar_v<typename MessageType::value_type>,
        "Message type must contain scalar values");
    using ValueType = typename MessageType::value_type;
    auto msg = Messaging::Message {};
    socket.recv(&msg);
    const auto first = msg.data<ValueType>();
    const auto last = first + msg.size() / sizeof(ValueType);
    const auto more = socket.getsockopt<std::int64_t>(ZMQ_RCVMORE);
    return {MessageType(first, last), more != 0};
}

/** \brief Receive all parts of a multipart message
 *
 * If \p socket is router or dealer, the first part is ignored. For router
 * socket this is expected to be called after extracting the identity frame by
 * other means.
 *
 * \tparam MessageType the type of the message. It must be a standard container
 * (blob, vector, string etc.) holding scalar values.
 *
 * \param out output iterator the messages are written to
 * \param socket socket used to receive the messages
 *
 * \deprecated Do not use for router sockets. More robust identity handling
 * implemented in the MessageQueue class which the only class in the messaging
 * framework currently properly supporting router sockets.
 *
 * \deprecated Use recvMultipart(), ZMQ API and serializers for more flexibility.
 */
template<typename MessageType, typename OutputIterator>
void recvAll(OutputIterator out, Messaging::Socket& socket)
{
    auto more = recvEmptyFrameIfNecessary(socket);
    while (more) {
        auto message = recvMessage<MessageType>(socket);
        *out++ = std::move(message.first);
        more = message.second;
    }
}

/** \brief Create a pair of mutually connected sockets
 *
 * This function creates two PAIR sockets connected to each other. The first
 * socket in the pair binds and the second socket in the pair connects to the
 * given \p endpoint.
 *
 * \param context the ZeroMQ context
 * \param endpoint the common endpoint of the sockets
 *
 * \return std::pair containing two mutually connected sockets
 */
inline std::pair<Messaging::Socket, Messaging::Socket> createSocketPair(
    Messaging::MessageContext& context, const std::string& endpoint)
{
    auto ret = std::pair {
        Messaging::Socket {context, Messaging::SocketType::pair},
        Messaging::Socket {context, Messaging::SocketType::pair},
    };
    ret.first.bind(endpoint.data());
    ret.second.connect(endpoint.data());
    return ret;
}

}
}

#endif // MESSAGING_MESSAGEHELPER_HH_
