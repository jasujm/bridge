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

#include <string>

namespace Bridge {
namespace Messaging {

/** \brief Send message through socket
 *
 * \param socket the socket to use for sending the message
 * \param message the message to be sent (typically a string)
 */
template<typename MessageType>
void sendMessage(zmq::socket_t& socket, const MessageType& message)
{
    socket.send(std::begin(message), std::end(message));
}

/** \brief Receive message sent through socket
 *
 * \tparam CharType the character type of the message
 *
 * \param socket the socket to receive the message from
 *
 * \return std::basic_string<CharType> object containing the message
 */
template<typename CharType = char>
std::basic_string<CharType> recvMessage(zmq::socket_t& socket)
{
    auto msg = zmq::message_t {};
    socket.recv(&msg);
    return {msg.data<CharType>(), msg.size() / sizeof(CharType)};
}

}
}

#endif // MESSAGING_MESSAGEUTILITY_HH_
