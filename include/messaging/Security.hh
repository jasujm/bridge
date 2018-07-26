/** \file
 *
 * \brief Definition of CurveZMQ security utilities
 *
 * This header defines utilities for configuring CurveZMW based communication
 * between nodes (http://curvezmq.org/).
 *
 * \warning This feature is still experimental. The keypair is currently the
 * test keypair defined in the ZeroMQ manual page
 * (http://api.zeromq.org/4-2:zmq-curve). It does not provide actual security.
 */

#ifndef MESSAGING_SECURITY_HH_
#define MESSAGING_SECURITY_HH_

#include <zmq.hpp>

namespace Bridge {
namespace Messaging {

/** \brief Setup socket as curve server
 *
 * This function sets the appropriate socket options on \p socket to make it act
 * as curve server.
 *
 * \warning This feature is still experimental. The keypair is currently the
 * test keypair defined in the ZeroMQ manual page
 * (http://api.zeromq.org/4-2:zmq-curve). It does not provide actual security.
 *
 * \param socket the socket that will act as curve server
 */
void setupCurveServer(zmq::socket_t& socket);

/** \brief Setup socket as curve client
 *
 * This function sets the appropriate socket options on \p socket to make it act
 * as curve client.
 *
 * \warning This feature is still experimental. The keypair is currently the
 * test keypair defined in the ZeroMQ manual page
 * (http://api.zeromq.org/4-2:zmq-curve). It does not provide actual security.
 *
 * \param socket the socket that will act as curve client
 */
void setupCurveClient(zmq::socket_t& socket);

}
}

#endif // MESSAGING_SECURITY_HH_
