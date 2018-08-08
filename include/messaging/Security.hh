/** \file
 *
 * \brief Definition of CurveZMQ security utilities
 *
 * This header defines utilities for configuring CurveZMW based communication
 * between nodes (http://curvezmq.org/).
 */

#ifndef MESSAGING_SECURITY_HH_
#define MESSAGING_SECURITY_HH_

#include "Blob.hh"

#include <zmq.hpp>

#include <cstddef>
#include <optional>
#include <string_view>

namespace Bridge {
namespace Messaging {

/** \brief Expected size of CurveZMQ keys
 *
 * ZeroMQ API accepts curve keys as 32 byte buffers. This constant defines the
 * expected size of the key.
 */
constexpr std::size_t EXPECTED_CURVE_KEY_SIZE = 32;

/** \brief Collection of CurveZMQ keys
 */
struct CurveKeys {
    Blob serverKey;  ///< \brief Server public key (clients only)
    Blob secretKey;  ///< \brief Secret key
    Blob publicKey;  ///< \brief Public key
};

/** \brief Decode Z85 encoded CURVE key
 *
 * \param encodedKey Z85 encoded key (40 characters)
 *
 * \return Blob containing the decoded key (32 bytes), or an empty blob if \p
 * encodedKey is invalid (wrong length or characters)
 */
Blob decodeKey(std::string_view encodedKey);

/** \brief Setup socket as curve server
 *
 * This function sets the appropriate socket options on \p socket to make it act
 * as curve server.
 *
 * \param socket the socket that will act as curve server
 * \param keys the CurveZMQ keys
 *
 * \todo Do not fail silently if \p keys are wrong size
 */
void setupCurveServer(zmq::socket_t& socket, const CurveKeys* keys);

/** \brief Setup socket as curve client
 *
 * This function sets the appropriate socket options on \p socket to make it act
 * as curve client.
 *
 * \param socket the socket that will act as curve client
 * \param keys the CurveZMQ keys
 *
 * \todo Do not fail silently if \p keys are wrong size
 */
void setupCurveClient(zmq::socket_t& socket, const CurveKeys* keys);

}
}

#endif // MESSAGING_SECURITY_HH_
