/** \file
 *
 * \brief Definition of command utilities
 *
 * Bridge messaging patterns use commands composed of string describing the
 * command and an arbitrary number of serialized objects as parameters to the
 * command, sent as multipart message. This file defines utilities for sending
 * commands.
 */

#ifndef MESSAGING_COMMANDUTILITY_H_
#define MESSAGING_COMMANDUTILITY_H_

#include "messaging/MessageUtility.hh"
#include "messaging/SerializationUtility.hh"

#include <algorithm>
#include <array>
#include <string>
#include <type_traits>

namespace Bridge {
namespace Messaging {

/** \brief Send command through ZeroMQ socket
 *
 * This function serializes all parameters of the command using \p
 * serializer. It then sends multipart message consisting of \p command and
 * all its serialized \p params using \p socket.
 *
 * \tparam SocketIterator An input iterator that, when dereferenced, returns a
 * reference to zmq::socket_t object.
 *
 * \param firstSocket iterator to the first socket to send the command to
 * \param lastSocket iterator one past the last socket to send the command to
 * \param serializer serialization policy for the command parameters
 * \param command the command sent as the first part of the message
 * \param params the parameters serialized and sent as the subsequent parts of
 * the message
 *
 * \sa \ref serializationpolicy
 */
template<
    typename SocketIterator, typename SerializationPolicy,
    typename CommandString, typename... Params>
void sendCommand(
    SocketIterator firstSocket,
    SocketIterator lastSocket,
    SerializationPolicy&& serializer,
    CommandString&& command,
    Params&&... params)
{
    using ParamString = std::common_type_t<
        decltype(serializer.serialize(params))...>;
    std::array<ParamString, sizeof...(params)> params_;
    serializeAll(
        params_.begin(), serializer,
        std::forward<Params>(params)...);
    std::for_each(
        firstSocket, lastSocket,
        [&command, &params_](zmq::socket_t& socket)
        {
            sendEmptyFrameIfNecessary(socket);
            sendMessage(socket, command, true);
            sendMessage(socket, params_.begin(), params_.end());
        });
}

/** \brief Send command throught ZeroMQ socket
 *
 * \note This overload is for commands without parameters, so
 * SerializationPolicy is ignored.
 *
 * \tparam SocketIterator An input iterator that, when dereferenced, returns a
 * reference to zmq::socket_t object.
 *
 * \param firstSocket iterator to the first socket to send the command to
 * \param lastSocket iterator one past the last socket to send the command to
 * \param command the command sent
 */
template<
    typename SocketIterator, typename SerializationPolicy,
    typename CommandString>
void sendCommand(
    SocketIterator firstSocket,
    SocketIterator lastSocket,
    SerializationPolicy&&,
    CommandString&& command)
{
    std::for_each(
        firstSocket, lastSocket,
        [&command](zmq::socket_t& socket)
        {
            sendEmptyFrameIfNecessary(socket);
            sendMessage(socket, command);
        });
}

}
}

#endif // MESSAGING_COMMANDUTILITY_H_
