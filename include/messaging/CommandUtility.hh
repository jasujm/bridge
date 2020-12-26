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

#include <boost/iterator/function_output_iterator.hpp>

#include <utility>

namespace Bridge {
namespace Messaging {

/// \cond DOXYGEN_IGNORE

namespace Impl {

template<typename OutputIterator, typename SerializationPolicy>
OutputIterator makeParametersHelper(OutputIterator out, SerializationPolicy&&)
{
    return out;
}

template<
    typename OutputIterator, typename SerializationPolicy,
    typename Head, typename... Rest>
OutputIterator makeParametersHelper(
    OutputIterator out, SerializationPolicy&& serializer,
    Head&& head, Rest&&... rest)
{
    using std::get;
    *out++ = messageFromContainer(get<0>(std::forward<Head>(head)));
    *out++ = messageFromContainer(
        serializer.serialize(get<1>(std::forward<Head>(head))));
    return makeParametersHelper(
        std::move(out),
        std::forward<SerializationPolicy>(serializer),
        std::forward<Rest>(rest)...);
}

}

/** \brief Build command parameter frames
 *
 * \param out the output iterator the parts are written to
 * \param serializer the serialization policy, see \ref serializationpolicy
 * \param params the key–value pairs making the subsequent parts of the message
 *
 * \sa makeCommandMessage(), makeEventMessage()
 */
template<
    typename SerializationPolicy, typename... Params, typename OutputIterator>
OutputIterator makeCommandParameters(
    OutputIterator out, SerializationPolicy&& serializer, Params&&... params)
{
    return Impl::makeParametersHelper(
        std::move(out), std::forward<SerializationPolicy>(serializer),
        std::forward<Params>(params)...);
}

/// \endcond DOXYGEN_IGNORE

/** \brief Build command message
 *
 * This function is intended to make \ref bridgeprotocolcontrolmessage
 * as defined in the bridge protocol. A command message consists of
 * tag frame, command frame and a variable number of parameters in
 * form of key–value pairs. The parameters are serialized using \p
 * serializer and output to \p out.
 *
 * For example, with \p serializer being a serializer that does lexical
 * casting:
 *
 * \code{.cc}
 * using namespace std::string_literals;
 * auto parts = std::vector<Messaging::Message> {};
 * makeCommandMessage(
 *     std::back_inserter(parts), serializer,
 *     "tag"s, "command"s, std::pair {"argument"s, 123});
 * \endcode
 *
 * \c parts will contain frames “tag”, “command”, “argument” and “123”.
 *
 * \param out the output iterator the parts are written to
 * \param serializer the serialization policy, see \ref serializationpolicy
 * \param tag the content of the tag frame
 * \param command the content of the command frame
 * \param params the key–value pairs making the subsequent parts of the message
 *
 * \return \p out
 */
template<
    typename SerializationPolicy, typename String, typename... Params,
    typename OutputIterator>
OutputIterator makeCommandMessage(
    OutputIterator out, SerializationPolicy&& serializer, const String& tag,
    const String& command, Params&&... params)
{
    *out++ = messageFromContainer(tag);
    *out++ = messageFromContainer(command);
    return Impl::makeParametersHelper(
        std::move(out), std::forward<SerializationPolicy>(serializer),
        std::forward<Params>(params)...);
}

/** \brief Send command message
 *
 * This is a convenience function for building a command and sending
 * it through \p socket. The mechanism is the same as in
 * makeCommandMessage() expect instead of outputting the parts to an
 * iterator, they are sent through directly (prepended by empty frame
 * if it is dealer or router).
 *
 * \param socket the socket to send the command to
 * \param serializer the serialization policy, see \ref serializationpolicy
 * \param tag the content of the tag frame
 * \param command the content of the command frame
 * \param params the key–value pairs making the subsequent parts of the message
 */
template<typename SerializationPolicy, typename String, typename... Params>
void sendCommandMessage(
    Socket& socket, SerializationPolicy&& serializer, const String& tag,
    const String& command, Params&&... params)
{
    auto count = 0u;
    sendEmptyFrameIfNecessary(socket);
    makeCommandMessage(
        boost::make_function_output_iterator(
            [&socket, &count](const Message& msg)
                {
                    // const_cast because boost function output iterator passes the
                    // parameter as const although it's unnecessary here
                    sendMessage(
                        socket, std::move(const_cast<Message&>(msg)),
                        count <= 2 * sizeof...(params));
                    ++count;
                }),
        std::forward<SerializationPolicy>(serializer),
        tag, command, std::forward<Params>(params)...);
}

/** \brief Build event message
 *
 * This function is intended to make \ref bridgeprotocoleventmessage
 * as defined in the bridge protocol. An event message consists of
 * event frame and a variable number of parameters in form of
 * key–value pairs. The parameters are serialized using \p serializer
 * and output to \p out.
 *
 * For example, with \p serializer being a serializer that does lexical
 * casting:
 *
 * \code{.cc}
 * using namespace std::string_literals;
 * auto parts = std::vector<Messaging::Message> {};
 * makeEventMessage(
 *     std::back_inserter(parts), serializer,
 *     "event"s, std::pair {"argument"s, 123});
 * \endcode
 *
 * \c parts will contain frames “event”, “argument” and “123”.
 *
 * \param out the output iterator the parts are written to
 * \param serializer the serialization policy, see \ref serializationpolicy
 * \param event the content of the event frame
 * \param params the key–value pairs making the subsequent parts of the message
 *
 * \return \p out
 */
template<
    typename SerializationPolicy, typename String, typename... Params,
    typename OutputIterator>
OutputIterator makeEventMessage(
    OutputIterator out, SerializationPolicy&& serializer, const String& event,
    Params&&... params)
{
    *out++ = messageFromContainer(event);
    return Impl::makeParametersHelper(
        std::move(out), std::forward<SerializationPolicy>(serializer),
        std::forward<Params>(params)...);
}

/** \brief Send event message
 *
 * This is a convenience function for building a event and sending
 * it through \p socket. The mechanism is the same as in
 * makeEventMessage() expect instead of outputting the parts to an
 * iterator, they are sent through directly (prepended by empty frame
 * if it is dealer or router).
 *
 * \param socket the socket to send the event to
 * \param serializer the serialization policy, see \ref serializationpolicy
 * \param event the contents of the event frame
 * \param params the key–value pairs making the subsequent parts of the message
 */
template<typename SerializationPolicy, typename String, typename... Params>
void sendEventMessage(
    Socket& socket, SerializationPolicy&& serializer, const String& event,
    Params&&... params)
{
    auto count = 0u;
    sendEmptyFrameIfNecessary(socket);
    makeEventMessage(
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
        event, std::forward<Params>(params)...);
}

}
}

#endif // MESSAGING_COMMANDUTILITY_H_
