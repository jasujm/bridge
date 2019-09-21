/** \file
 *
 * \brief Bridge messaging framework socket definitions
 */

#ifndef MESSAGING_SOCKETS_HH_
#define MESSAGING_SOCKETS_HH_

#include <iterator>
#include <memory>

#include <zmq.hpp>

namespace Bridge {
namespace Messaging {

/** \brief ZeroMQ context type
 */
using MessageContext = zmq::context_t;

/** \brief ZeroMQ socket type
 */
using Socket = zmq::socket_t;

/** \brief Enumeration containing types for ZeroMQ sockets
 */
using SocketType = zmq::socket_type;

/** \brief ZeroMQ socket type with shared ownership
 */
using SharedSocket = std::shared_ptr<Socket>;

/** \brief ZeroMQ message type
 */
using Message = zmq::message_t;

/** \brief Make shared ZeroMQ socket
 *
 * This is a wrapper over \c std::make_shared for Socket.
 *
 * \param args The arguments to be forwarded to \c std::make_shared
 *
 * \return SharedSocket constructed from the arguments
 */
template<typename... Args>
auto makeSharedSocket(Args&&... args)
{
    return std::make_shared<Socket>(std::forward<Args>(args)...);
}

/** \brief ZeroMQ related exception
 */
using SocketError = zmq::error_t;

/** \brief ZeroMQ socket pollitem
 */
using Pollitem = zmq::pollitem_t;

/** \brief Wrapper over zmq_poll
 *
 * \param pollitems list of Pollitem objects
 * \param timeout the timeout
 *
 * \return Number of events on the pollitems
 */
template<typename PollitemRange, typename Timeout = long>
auto pollSockets(PollitemRange& pollitems, Timeout timeout = -1)
{
    return zmq::poll(std::data(pollitems), std::size(pollitems), timeout);
}

}
}

#endif // MESSAGING_SOCKETS_HH_
