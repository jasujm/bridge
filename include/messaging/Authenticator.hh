/** \file
 *
 * \brief Definition of Bridge::Messaging::Authenticator class
 */

#ifndef MESSAGING_AUTHENTICATOR_HH_
#define MESSAGING_AUTHENTICATOR_HH_

#include "messaging/Identity.hh"
#include "Blob.hh"
#include "BlobMap.hh"
#include "Thread.hh"

#include <boost/operators.hpp>
#include <boost/noncopyable.hpp>
#include <zmq.hpp>

#include <string>

namespace Bridge {
namespace Messaging {

/** \brief ZAP implementation
 *
 * This class implements the ZAP protocol (https://rfc.zeromq.org/spec:27/ZAP/)
 * for authenticating incoming connections. It’s checked that the incoming
 * connections use CURVE mechanism. If they are, the connection is accepted. The
 * handler generates unique User-Id for the connection and publishes it along
 * with the CURVE public key for the application use.
 *
 * A worker thread is created for the ZAP handler. The thread is joined when the
 * Authenticator instance is destructed.
 *
 * Because a process may only contain one ZAP handler, only one instance of this
 * class should exist at time.
 */
class Authenticator : private boost::noncopyable {
public:

    /** \brief Mapping between public keys and node identities
     */
    using NodeMap = BlobMap<std::string>;

    /** \brief Create authenticator
     *
     * \param context ZeroMQ context
     * \param terminationSubscriber Socket that will receive notification about
     * termination of the thread
     * \param knownNodes Mapping between known public keys and their user IDs
     */
    Authenticator(
        zmq::context_t& context,
        zmq::socket_t terminationSubscriber,
        NodeMap knownNodes = {});

    /** \brief Block until the authenticator is ready
     *
     * The authenticator runs in a separate thread. Before ready it sets up a
     * socket for the ZAP protocol. Any thread that intends to accept
     * connections or should call this method before binding a socket.
     */
    void ensureRunning();

private:

    zmq::context_t& context;
    zmq::socket_t controlSocket;
    Thread worker;
};

}
}

#endif // MESSAGING_AUTHENTICATOR_HH_
