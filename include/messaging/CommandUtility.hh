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
 * \param socket socket used for sending the command
 * \param serializer \sa \ref serializationpolicy
 * \param command the command sent as the first part of the message
 * \param params the parameters serialized and sent as the subsequent parts of
 * the message
 */
template<
    typename SerializationPolicy, typename CommandString,
    typename... Params>
void sendCommand(
    zmq::socket_t& socket,
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
    sendMessage(socket, command, sizeof...(Params) > 0);
    sendMessage(socket, params_.begin(), params_.end());
}

}
}

#endif // MESSAGING_COMMANDUTILITY_H_
