/** \file
 *
 * \brief Definition of Bridge::Messaging::MessageBuffer
 */

#ifndef MESSAGING_MESSAGEBUFFER_HH_
#define MESSAGING_MESSAGEBUFFER_HH_

#include "messaging/MessageUtility.hh"
#include "Utility.hh"

#include <zmq.hpp>

#include <memory>
#include <sstream>
#include <tuple>
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
 *
 * \sa MessageBuffer
 */
template<
    typename Char,
    class Traits = std::char_traits<Char>,
    class Allocator = std::allocator<Char>>
class BasicMessageBuffer :
    public std::basic_stringbuf<Char, Traits, Allocator> {
public:

    /** \brief Create message buffer
     *
     * \param socket the socket used to synchronize contents
     */
    BasicMessageBuffer(std::shared_ptr<zmq::socket_t> socket);

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
};

template<typename Char, class Traits, class Allocator>
BasicMessageBuffer<Char, Traits, Allocator>::BasicMessageBuffer(
    std::shared_ptr<zmq::socket_t> socket) :
    socket {std::move(socket)}
{
}

template<typename Char, class Traits, class Allocator>
int BasicMessageBuffer<Char, Traits, Allocator>::sync()
{
    const auto& msg = this->str();
    if (!msg.empty()) {
        sendMessage(dereference(this->socket), msg);
        this->str({});
    }
    return 0;
}

template<typename Char, class Traits, class Allocator>
typename Traits::int_type BasicMessageBuffer<Char, Traits, Allocator>::
underflow()
{
    const auto super = std::basic_stringbuf<
        Char, Traits, Allocator>::underflow();
    if (super != Traits::eof()) {
        return super;
    }

    using String = std::basic_string<Char, Traits, Allocator>;
    auto msg = String {};
    auto more = false;
    while (msg.empty()) {
        std::tie(msg, more) = recvMessage<String>(dereference(this->socket));
    }
    this->str(msg);
    return Traits::to_int_type(msg.front());
}

/** \brief BasicMessageBuffer specialization for regular char
 */
using MessageBuffer = BasicMessageBuffer<char>;

}
}

#endif // MESSAGING_MESSAGEBUFFER_HH_
