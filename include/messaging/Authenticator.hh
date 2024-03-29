/** \file
 *
 * \brief Definition of Bridge::Messaging::Authenticator class
 */

#ifndef MESSAGING_AUTHENTICATOR_HH_
#define MESSAGING_AUTHENTICATOR_HH_

#include "messaging/Identity.hh"
#include "messaging/Sockets.hh"
#include "Blob.hh"
#include "BlobMap.hh"
#include "Thread.hh"

#include <boost/noncopyable.hpp>

namespace Bridge {
namespace Messaging {

/** \brief ZAP implementation
 *
 * This class implements the ZAP protocol
 * (https://rfc.zeromq.org/spec:27/ZAP/) for authenticating incoming
 * connections. It’s checked that the incoming connections use CURVE
 * mechanism. If they are, the connection is accepted. Each connection
 * is assigned an User-Id. The user id is either the ID associated
 * with one of the known nodes, or otherwise a unique name derived
 * from their public key.
 *
 * A worker thread is created for the ZAP handler. The thread is
 * joined when the Authenticator instance is destructed.
 *
 * Because a process may only contain one ZAP handler, only one
 * instance of this class should exist at time.
 */
class Authenticator : private boost::noncopyable {
public:

    /** \brief Mapping between public keys and node identities
     */
    using NodeMap = BlobMap<UserId>;

    /** \brief Create authenticator
     *
     * \param context ZeroMQ context
     * \param terminationSubscriber Socket that will receive notification about
     * termination of the thread
     * \param knownNodes Mapping between known public keys and their user IDs
     */
    Authenticator(
        MessageContext& context, Socket terminationSubscriber,
        NodeMap knownNodes = {});

    /** \brief Block until the authenticator is ready
     *
     * The authenticator runs in a separate thread. Before ready it sets up a
     * socket for the ZAP protocol. Any thread that intends to accept
     * connections or should call this method before binding a socket.
     */
    void ensureRunning();

    /** \brief Add new known node
     *
     * This method call a node having public key \p key and user ID \p userId to
     * the list of known nodes. The authenticator will recognize the new node
     * once the function call returns.
     *
     * \param key the public key of the new node
     * \param userId the user ID of the new node
     */
    void addNode(ByteSpan key, UserIdView userId);

private:

    MessageContext& context;
    Socket controlSocket;
    Thread worker;
};

}
}

#endif // MESSAGING_AUTHENTICATOR_HH_
