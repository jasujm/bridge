/** \file
 *
 * \brief Definition of CurveZMQ security utilities
 *
 * This header defines utilities for configuring CurveZMW based communication
 * between nodes (http://curvezmq.org/).
 */

#ifndef MESSAGING_SECURITY_HH_
#define MESSAGING_SECURITY_HH_

#include "messaging/Sockets.hh"
#include "Blob.hh"

#include <string_view>

namespace Bridge {
namespace Messaging {

/** \brief Expected size of CurveZMQ keys
 *
 * ZeroMQ API accepts curve keys as 32 byte buffers. This constant defines the
 * expected size of the key.
 */
constexpr auto EXPECTED_CURVE_KEY_SIZE = 32;

/** \brief CurveZMQ keypair
 */
struct CurveKeys {
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
 * as curve server. If \p keys is \c nullptr, does nothing.
 *
 * \param socket the socket that will act as curve server
 * \param keys the server keypair
 *
 * \throw std::invalid_argument if the secret key in \p keys has
 * invalid length
 */
void setupCurveServer(Socket& socket, const CurveKeys* keys);

/** \brief Setup socket as curve client
 *
 * This function sets the appropriate socket options on \p socket to
 * make it act as curve client. If \p keys is \c nullptr or \p
 * serverKey is empty, does nothing.
 *
 * \param socket the socket that will act as curve client
 * \param keys the client keypair
 * \param serverKey the server public key
 *
 * \throw std::invalid_argument if any of the keys in \p keys or \p
 * serverKey has invalid length
 */
void setupCurveClient(
    Socket& socket, const CurveKeys* keys, ByteSpan serverKey);

}
}

#endif // MESSAGING_SECURITY_HH_
