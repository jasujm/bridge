/** \file
 *
 * \brief Definition of Bridge::Messaging::MessageBuffer
 */

#ifndef MESSAGING_MESSAGEBUFFER_HH_
#define MESSAGING_MESSAGEBUFFER_HH_

#include "messaging/MessageUtility.hh"
#include "messaging/SynchronousExecutionPolicy.hh"
#include "Utility.hh"

#include <zmq.hpp>

#include <memory>
#include <sstream>
#include <utility>

namespace Bridge {
namespace Messaging {

/** \brief String buffer that sends and receives messages through ZeroMQ sockets
 *
 * BasicMessageBuffer is derived from std::string_buffer and can be used like
 * any string buffer (building and extracting data from string). When the
 * output is synchronized, the contents of the buffer is sent through a ZeroMQ
 * socket. When it underflows, new content is receivec from the socket. Sends
 * and receives are blocking.
 *
 * The intended use of BasicMessageBuffer is to create input and output
 * streams that act as frontends of ZeroMQ sockets.
 *
 * \tparam Char the character type
 * \tparam Traits the character traits
 * \tparam Allocator allocator for the string
 * \tparam ExecutionContext the execution context
 *
 * \sa MessageBuffer
 */
template<
    typename Char,
    typename Traits = std::char_traits<Char>,
    typename Allocator = std::allocator<Char>,
    typename ExecutionContext = SynchronousExecutionContext>
class BasicMessageBuffer :
        public std::basic_stringbuf<Char, Traits, Allocator> {
public:

    /** \brief Create message buffer
     *
     * \param socket the socket used to synchronize contents
     * \param context the execution context
     */
    BasicMessageBuffer(
        std::shared_ptr<zmq::socket_t> socket,
        ExecutionContext context = {});

protected:

    /** \brief Send current contents of the buffer
     *
     * Synchronizes the controlled character sequence by sending it through
     * the socket given as parameter to BasicMessageBuffer(). This method is
     * intended to be called by the output stream when it is flushed.
     */
    int sync() override;

    /** \brief Retrieve new content for the buffer
     *
     * Ensures that there are character available by retrieving message
     * through the socket given as parameter to BasicMessageBuffer() and
     * filling the controlled character sequence by that message. This method
     * is intended to be called by the input stream when it runs out of
     * characters.
     */
    typename Traits::int_type underflow() override;

private:

    std::shared_ptr<zmq::socket_t> socket;
    ExecutionContext context;
};

template<
    typename Char, typename Traits, typename Allocator, typename ExecutionContext>
BasicMessageBuffer<Char, Traits, Allocator, ExecutionContext>::
BasicMessageBuffer(
    std::shared_ptr<zmq::socket_t> socket, ExecutionContext context) :
    socket {std::move(socket)},
    context {std::move(context)}
{
}

template<
    typename Char, typename Traits, typename Allocator, typename ExecutionContext>
int BasicMessageBuffer<Char, Traits, Allocator, ExecutionContext>::sync()
{
    const auto& msg = this->str();
    if (!msg.empty()) {
        dereference(this->socket).send(msg.data(), msg.size() * sizeof(Char));
        this->str({});
    }
    return 0;
}

template<
    typename Char, typename Traits, typename Allocator, typename ExecutionContext>
typename Traits::int_type
BasicMessageBuffer<Char, Traits, Allocator, ExecutionContext>::underflow()
{
    const auto super = std::basic_stringbuf<
        Char, Traits, Allocator>::underflow();
    if (super != Traits::eof()) {
        return super;
    }

    using String = std::basic_string<Char, Traits, Allocator>;
    auto msg = zmq::message_t {};
    auto size = 0u;
    do {
        ensureSocketReadable(context, socket);
        dereference(socket).recv(&msg);
        size = msg.size();
    } while (size == 0u);
    const auto* data = msg.data<Char>();
    const auto count = size / sizeof(Char);
    this->str(String(data, count));
    return Traits::to_int_type(*data);
}

/** \brief BasicMessageBuffer specialization for regular char
 */
template<typename ExecutionContext>
using MessageBuffer = BasicMessageBuffer<
    char, std::char_traits<char>, std::allocator<char>, ExecutionContext>;

/** \brief MessageBuffer specialization for synchronous execution policy
 */
using SynchronousMessageBuffer = MessageBuffer<SynchronousExecutionContext>;

/** \brief Standard stream wrapping BasicMessageBuffer as its buffer
 *
 * \tparam BaseStream the standard stream it inherits from
 * \tparam Char the character type
 * \tparam Traits the character traits
 * \tparam Allocator allocator for the string
 * \tparam ExecutionContext the execution context
 */
template<
    template<typename...> typename BaseStream, typename Char,
    typename Traits = std::char_traits<Char>,
    typename Allocator = std::allocator<Char>,
    typename ExecutionContext = SynchronousExecutionContext>
class BasicMessageStream : public BaseStream<Char, Traits>
{
public:

    /** \brief Create message stream
     *
     * \param socket the socket used to synchronize contents
     * \param context the execution context
     */
    BasicMessageStream(
        std::shared_ptr<zmq::socket_t> socket,
        ExecutionContext context = {});

    /** \todo Implement if needed
     */
    BasicMessageStream(BasicMessageStream&&) = delete;

private:

    BasicMessageBuffer<Char, Traits, Allocator, ExecutionContext> buffer;
};

template<
    template<typename...> typename BaseStream, typename Char, typename Traits,
    typename Allocator, typename ExecutionContext>
BasicMessageStream<
    BaseStream, Char, Traits, Allocator, ExecutionContext>::
BasicMessageStream(
    std::shared_ptr<zmq::socket_t> socket, ExecutionContext context) :
    buffer {std::move(socket), std::move(context)}
{
    this->init(&buffer);
}

/** \brief BasicMessageStream specialization for std::istream
 */
template<typename ExecutionContext>
using MessageIStream = BasicMessageStream<
    std::basic_istream, char, std::char_traits<char>, std::allocator<char>,
    ExecutionContext>;

/** \brief MessageIStream specialization for synchronous execution policy
 */
using SynchronousMessageIStream = MessageIStream<SynchronousExecutionContext>;

/** \brief BasicMessageStream specialization for std::ostream
 */
template<typename ExecutionContext>
using MessageOStream = BasicMessageStream<
    std::basic_ostream, char, std::char_traits<char>, std::allocator<char>,
    ExecutionContext>;

/** \brief MessageOStream specialization for synchronous execution policy
 */
using SynchronousMessageOStream = MessageOStream<SynchronousExecutionContext>;

}
}

#endif // MESSAGING_MESSAGEBUFFER_HH_
