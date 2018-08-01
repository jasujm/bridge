/** \file
 *
 * \brief Definition of CurveZMQ security utilities
 *
 * This header defines utilities for configuring CurveZMW based communication
 * between nodes (http://curvezmq.org/).
 */

#ifndef MESSAGING_SECURITY_HH_
#define MESSAGING_SECURITY_HH_

#include <zmq.hpp>

#include <cstddef>
#include <optional>

namespace Bridge {
namespace Messaging {

/** \brief Expected size of CurveZMQ keys
 *
 * ZeroMQ API accepts curve keys as null terminated Z85 encoded strings. This
 * constant defines the expected size of the encoded key (excuding the null
 * character).
 */
constexpr std::size_t EXPECTED_CURVE_KEY_SIZE = 40;

/** \brief Collection of CurveZMQ keys
 */
struct CurveKeys {
    std::string serverKey;  ///< \brief Server public key (clients only)
    std::string secretKey;  ///< \brief Secret key
    std::string publicKey;  ///< \brief Public key
};

/** \brief Setup socket as curve server
 *
 * This function sets the appropriate socket options on \p socket to make it act
 * as curve server.
 *
 * \param socket the socket that will act as curve server
 * \param keys the CurveZMQ keys
 */
void setupCurveServer(zmq::socket_t& socket, const CurveKeys* keys);

/** \brief Setup socket as curve client
 *
 * This function sets the appropriate socket options on \p socket to make it act
 * as curve client.
 *
 * \param socket the socket that will act as curve client
 * \param keys the CurveZMQ keys
 */
void setupCurveClient(zmq::socket_t& socket, const CurveKeys* keys);

}
}

#endif // MESSAGING_SECURITY_HH_
