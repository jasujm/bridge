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
 * \param first iterator to the begin of the messages to send
 * \param last iterator to the end of the messages to send
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

/** \brief Receive message sent through socket
 *
 * \tparam CharType the character type of the string the message consists of
 *
 * \param socket the socket used to receive the message
 *
 * \return A pair containing the following
 *   - string containing the message
 *   - boolean indicating whether there are more parts in the message
 */
template<typename CharType = char>
std::pair<std::basic_string<CharType>, bool> recvMessage(zmq::socket_t& socket)
{
    auto msg = zmq::message_t {};
    socket.recv(&msg);
    const auto more = socket.getsockopt<std::int64_t>(ZMQ_RCVMORE);
    return {{msg.data<CharType>(), msg.size() / sizeof(CharType)}, more != 0};
}

/** \brief Receive all parts of a multipart message
 *
 * \tparam CharType the character type of the string the messages consist of
 *
 * \param out output iterator the messages are written to
 * \param socket socket used to receive the messages
 */
template<typename CharType = char, typename OutputIterator>
void recvAll(OutputIterator out, zmq::socket_t& socket)
{
    auto more = true;
    while (more) {
        auto&& message = recvMessage<CharType>(socket);
        *out++ = std::move(message.first);
        more = message.second;
    }
}

}
}

#endif // MESSAGING_MESSAGEUTILITY_HH_