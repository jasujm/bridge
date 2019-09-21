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

#include "messaging/Sockets.hh"
#include "Blob.hh"

#include <boost/endian/conversion.hpp>

#include <iterator>
#include <limits>
#include <type_traits>
#include <utility>

namespace Bridge {
namespace Messaging {

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
inline void sendEmptyFrameIfNecessary(Socket& socket)
{
    const auto type = socket.getsockopt<SocketType>(ZMQ_TYPE);
    if (type == SocketType::router || type == SocketType::dealer) {
        socket.send(Message {}, ZMQ_SNDMORE);
    }
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
inline bool recvEmptyFrameIfNecessary(Socket& socket)
{
    const auto type = socket.getsockopt<SocketType>(ZMQ_TYPE);
    if (type == SocketType::router || type == SocketType::dealer) {
        auto empty_frame = Message {};
        socket.recv(&empty_frame);
        return empty_frame.more();
    }
    return true;
}

/** \brief Send multiple ZMQ frames as single multipart message
 *
 * This function takes a range of Message objects as input and sends them
 * using \p socket as single multipart message.
 *
 * \tparam MessageIterator input iterator over Message objects
 *
 * \param socket the socket
 * \param first iterator to the first frame to be sent
 * \param last iterator one past the last frame to be sent
 * \param flags flags to be passed to socket.send() method
 *
 * \note ZMQ_SNDMORE flag is always or’d with \p flags for all frames except the
 * last one. ZMQ_SNDMORE can still be passed as \p flags to this function, and
 * it will then be applied to the last frame too, allowing the multipart message
 * to continue after the parts sent using the function invocation.
 */
template<typename MessageIterator>
void sendMultipart(
    Socket& socket, MessageIterator first, MessageIterator last,
    int flags = 0)
{
    while (first != last) {
        const auto next = std::next(first);
        socket.send(*first, (next == last) ? flags : (flags | ZMQ_SNDMORE));
        first = next;
    }
}

/** \brief Ignore the rest of the message
 *
 * This function repeatedly receives message parts from \p socket until there
 * are no more parts in the current message. The parts are immediately
 * discarded. At least one part is always received.
 *
 * \param socket the socket
 * \param maximumParts the maximum number of parts to discard
 *
 * \return number of frames discarded
 */
inline int discardMessage(
    Socket& socket, int maximumParts = std::numeric_limits<int>::max())
{
    auto n_parts = 0;
    char buf[0];
    do {
        if (n_parts >= maximumParts) {
            break;
        }
        static_cast<void>(socket.recv(&buf, 0));
        ++n_parts;
    } while (socket.getsockopt<int>(ZMQ_RCVMORE));
    return n_parts;
}

/** \brief Receive multipart ZMQ message as individual frames
 *
 * This function receives as many frames from \p socket as the next message
 * contains and writes them to \p out. The caller may specify maximum number of
 * parts received. If this limit is reached, the rest of the message is
 * discarded. This may be useful for example in case where \p out points to a
 * range capable of holding a limited number of messages only.
 *
 * \tparam MessageIterator output iterator accepting Message objects
 *
 * \param socket the socket
 * \param out the output iterator the frames are written to
 * \param maximumParts the maximum number of parts to receive
 *
 * \return number of parts received, including the ignored parts
 */
template<typename MessageIterator>
std::pair<MessageIterator, int> recvMultipart(
    Socket& socket, MessageIterator out, int maximumParts = std::numeric_limits<int>::max())
{
    auto n_parts = 0;
    auto more = true;
    while (more && n_parts < maximumParts) {
        auto next_message = Message {};
        socket.recv(&next_message);
        more = next_message.more();
        *out++ = std::move(next_message);
        ++n_parts;
    }
    if (more) {
        n_parts += discardMessage(socket);
    }
    return {out, n_parts};
}

/** \brief Forward message from one socket to another
 *
 * This function repeatedly receives messages from \p fromSocket and sends them
 * to \p toSocket until there are no more parts in the current message. At least
 * one part is always received.
 *
 * \param fromSocket the socket the message parts are received from
 * \param toSocket the socket the message parts are sent to
 */
inline void forwardMessage(Socket& fromSocket, Socket& toSocket)
{
    auto more = true;
    auto msg = Message {};
    while (more) {
        fromSocket.recv(&msg);
        more = msg.more();
        toSocket.send(msg, more ? ZMQ_SNDMORE : 0);
    }
}

/** \brief Get view to the bytes \p message consists of
 *
 * \param message the message
 *
 * \return ByteSpan object over the bytes \p message consists of
 */
inline ByteSpan messageView(const Message& message)
{
    const auto* data = message.data<ByteSpan::value_type>();
    const auto size = static_cast<ByteSpan::index_type>(
        message.size() * sizeof(*data));
    return {data, size};
}

/** \brief Create ZMQ message from the content of a container
 *
 * Given a contiguous \p container of trivially copyable elements, create and
 * return a Message object containing its contents as data.
 *
 * \param container the container
 *
 * \return message containing the contents of \p container as its data
 */
Message messageFromContainer(const auto& container)
{
    const auto* data = std::data(container);
    using ValueType = std::remove_cv_t<
        std::remove_reference_t<decltype(*data)>>;
    static_assert(
        std::is_trivially_copyable_v<ValueType>,
        "Argument must contain trivially copyable elements");
    const auto size = std::size(container) * sizeof(ValueType);
    return {data, size};
}

/** \brief Create ZMQ message from a value
 *
 * Given a trivially copyable \p value, create and return a Message object
 * containing its object representation as data.
 *
 * \param value the value
 *
 * \return message containing the object representation of \p value as its data
 */
Message messageFromValue(const auto& value)
{
    using ValueType = std::remove_cv_t<
        std::remove_reference_t<decltype(value)>>;
    static_assert(
        std::is_trivially_copyable_v<ValueType>,
        "Argument must be trivially copyable");
    return {std::addressof(value), sizeof(ValueType)};
}

}
}

#endif // MESSAGING_MESSAGEUTILITY_HH_
