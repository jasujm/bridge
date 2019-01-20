/** \file
 *
 * \brief Definition of Bridge::Messaging::Identity
 */

#ifndef MESSAGING_IDENTITY_HH_
#define MESSAGING_IDENTITY_HH_

#include "Blob.hh"

namespace Bridge {
namespace Messaging {

/** \brief Identity of a node
 *
 * Identity is used in multiple places to identify another nodes. The messaging
 * framework is based on ZeroMQ library wher enode identities are binary
 * strings, not necessarily consisting of printable characters in any
 * encoding. The usage of blobs as identities is inherited from ZeroMQ.
 */
using Identity = Blob;

}
}

#endif // MESSAGING_IDENTITY_HH_
