/** \file
 *
 * \brief Definition of Bridge::Messaging::Identity
 */

#ifndef MESSAGING_IDENTITY_HH_
#define MESSAGING_IDENTITY_HH_

#include "messaging/Sockets.hh"
#include "Blob.hh"

#include <boost/operators.hpp>

#include <iosfwd>
#include <string>
#include <string_view>

namespace Bridge {
namespace Messaging {

/** \brief User ID type
 *
 * \sa Identity
 */
using UserId = std::string;

/** \brief Non‐owning view over UserId
 */
using UserIdView = std::string_view;

/** \brief Routing ID type
 *
 * \sa Identity
 */
using RoutingId = Blob;

/** \brief Identity of a node
 *
 * Identity objects are used in multiple places to identify another nodes. The
 * identity consists of two parts.
 *
 * 1. User ID which is identity set by the application and generally based on a
 * list of known nodes. May be a placeholder name if the node is not known to
 * the application. Authentication and authorization should be performed by \ref
 * userId.
 * 2. Routing ID which is an identity attached to the connection either by the
 * remote node itself or the ZeroMQ framework. \ref routingId should be
 * considered as an epheremal opaque blob that can be used as a session token
 * instead of actual authentication.
 */
struct Identity : public boost::totally_ordered<Identity> {

    Identity() = default;

    /** \brief Create new identity object
     *
     * \param userId see \ref userId
     * \param routingId see \ref routingId
     */
    Identity(UserId userId, RoutingId routingId);

    UserId userId;        ///< User ID
    RoutingId routingId;  ///< Routing ID
};

/** \brief Retrieve identity from ZeroMQ message
 *
 * This method accepts a message and retrieves the identity for the related
 * connection. \p message is a payload frame for the message, and will be used
 * to retrieve the User-Id metadata to populate the Identity::userId
 * field. Optionally \p routerIdentityFrame may be an identity frame received
 * from a ROUTER socket (before the actual payload), and will be used to
 * populate the Identity::routingId field.
 *
 * \param message Message payload frame
 * \param routerIdentityFrame Optional identity frame from ROUTER socket
 *
 * \return Identity of the connection related to the message
 */
Identity identityFromMessage(Message& message, Message* routerIdentityFrame);

/** \brief Equality operator for identities
 */
bool operator==(const Identity&, const Identity&);

/** \brief Less than operator for identities
 */
bool operator<(const Identity&, const Identity&);

/** \brief Output an identity to stream
 *
 * \param os the output stream
 * \param identity the identity
 *
 * \return parameter \p os
 */
std::ostream& operator<<(std::ostream& os, const Identity& identity);

}
}

#endif // MESSAGING_IDENTITY_HH_
