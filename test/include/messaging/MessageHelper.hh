/** \file
 *
 * \brief Messaging utilities to be used in unit tests
 */

#ifndef MESSAGING_MESSAGEHELPER_HH_
#define MESSAGING_MESSAGEHELPER_HH_

#include "messaging/Sockets.hh"

#include <string>
#include <utility>

namespace Bridge {
namespace Messaging {

/** \brief Create a pair of mutually connected sockets
 *
 * This function creates two PAIR sockets connected to each other. The first
 * socket in the pair binds and the second socket in the pair connects to the
 * given \p endpoint.
 *
 * \param context the ZeroMQ context
 * \param endpoint the common endpoint of the sockets
 *
 * \return std::pair containing two mutually connected sockets
 */
inline std::pair<Socket, Socket> createSocketPair(
    MessageContext& context, const std::string& endpoint)
{
    auto ret = std::pair {
        Socket {context, SocketType::pair},
        Socket {context, SocketType::pair},
    };
    ret.first.bind(endpoint.data());
    ret.second.connect(endpoint.data());
    return ret;
}

}
}

#endif // MESSAGING_MESSAGEHELPER_HH_
