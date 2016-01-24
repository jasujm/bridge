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

#include <cstdint>
#include <string>
#include <utility>

namespace Bridge {
namespace Messaging {

/** \brief Send message through socket
 *
 * \param socket the socket to use for sending the message
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

/** \brief Receive message sent through socket
 *
 * \tparam CharType the character type of the string the message consists of
 *
 * \param socket the socket to receive the message from
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

}
}

#endif // MESSAGING_MESSAGEUTILITY_HH_
