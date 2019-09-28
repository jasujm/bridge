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
#include "Utility.hh"

#include <boost/function_output_iterator.hpp>

#include <utility>

namespace Bridge {
namespace Messaging {

/// \cond DOXYGEN_IGNORE
// These are helpers for implementing makeCommand()

namespace Impl {

template<typename OutputIterator, typename SerializationPolicy>
OutputIterator makeCommandHelper(OutputIterator out, SerializationPolicy&&)
{
    return out;
}

template<
    typename OutputIterator, typename SerializationPolicy,
    typename Head, typename... Rest>
OutputIterator makeCommandHelper(
    OutputIterator out, SerializationPolicy&& serializer,
    Head&& head, Rest&&... rest)
{
    using std::get;
    *out++ = messageFromContainer(get<0>(std::forward<Head>(head)));
    *out++ = messageFromContainer(
        serializer.serialize(get<1>(std::forward<Head>(head))));
    return makeCommandHelper(
        out,
        std::forward<SerializationPolicy>(serializer),
        std::forward<Rest>(rest)...);
}

}

/// \endcond DOXYGEN_IGNORE

/** \brief Build command from parameters
 *
 * Command is defined as command string followed by parameters consisting of
 * key–value pairs. This is helper function for building such command
 * sequences from the parameters. The parameters are serialized using \p
 * serializer and output to \p out.
 *
 * For example, with \p serializer being a serializer that does lexical
 * casting:
 *
 * \code{.cc}
 * using namespace std::string_literals;
 * auto parts = std::vector<Messaging::Message> {};
 * makeCommand(
 *     std::back_inserter(parts), serializer,
 *     "command"s, std::make_pair("argument"s, 123));
 * \endcode
 *
 * \p parts contains messages “command”, “argument” and “123”.
 *
 * \param out the output iterator the parts are written to
 * \param serializer the serialization policy, see \ref serializationpolicy
 * \param command the command sent as the first part of the message
 * \param params the key–value pairs making the subsequent parts of the message
 *
 * \return \p out
 */
template<
    typename SerializationPolicy, typename CommandString, typename... Params,
    typename OutputIterator>
OutputIterator makeCommand(
    OutputIterator out,
    SerializationPolicy&& serializer,
    CommandString&& command,
    Params&&... params)
{
    *out++ = messageFromContainer(command);
    return Impl::makeCommandHelper(
        std::move(out), std::forward<SerializationPolicy>(serializer),
        std::forward<Params>(params)...);
}

/** \brief Send command through ZeroMQ socket
 *
 * This is a convenience function for building a command and sending it
 * through ZeroMQ socket. The mechanism is the same as in makeCommand() expect
 * instead of outputting the parts to an iterator, they are sent through \p
 * socket (prepended by empty frame if it is dealer or router).
 *
 * \param socket the socket to send the command to
 * \param serializer the serialization policy, see \ref serializationpolicy
 * \param command the command sent as the first part of the message
 * \param params the key–value pairs making the subsequent parts of the message
 *
 * \sa \ref serializationpolicy
 */
template<
    typename SerializationPolicy, typename CommandString, typename... Params>
void sendCommand(
    Socket& socket, SerializationPolicy&& serializer, CommandString&& command,
    Params&&... params)
{
    auto count = 0u;
    sendEmptyFrameIfNecessary(socket);
    makeCommand(
        boost::make_function_output_iterator(
            [&socket, &count](const Message& msg)
            {
                // const_cast because boost function output iterator passes the
                // parameter as const although it's unnecessary here
                sendMessage(
                    socket, std::move(const_cast<Message&>(msg)),
                    count < 2 * sizeof...(params));
                ++count;
            }),
        std::forward<SerializationPolicy>(serializer),
        std::forward<CommandString>(command),
        std::forward<Params>(params)...);
}

}
}

#endif // MESSAGING_COMMANDUTILITY_H_
