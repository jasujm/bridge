/** \file
 *
 * \brief Definition of Bridge::CardServer::PeerEntry
 */

#ifndef CARDSERVER_PEERENTRY_HH_
#define CARDSERVER_PEERENTRY_HH_

#include "Blob.hh"
#include "messaging/Identity.hh"

#include <boost/operators.hpp>

#include <iosfwd>
#include <optional>
#include <string>

namespace Bridge {
namespace CardServer {

/** \brief Peer entry in card server protocol
 *
 * This struct is the internal representation of a peer card server (card
 * servers connect to each other as well as the controlling bridge
 * application). The init command in card server protocol consumes a list of
 * peer entries.
 *
 * \sa \ref cardserverprotocol
 */
struct PeerEntry : private boost::equality_comparable<PeerEntry> {
    Messaging::Identity identity;         ///< \brief Peer identity
    std::optional<std::string> endpoint;  ///< \brief Card server endpoint
    std::optional<Blob> serverKey;        ///< \brief The CurveZMQ server key

    PeerEntry() = default;

    /** \brief Create new peer entry
     *
     * \param identity see \ref identity
     * \param endpoint see \ref endpoint
     * \param serverKey see \ref serverKey
     */
    explicit PeerEntry(
        Messaging::Identity identity,
        std::optional<std::string> endpoint = std::nullopt,
        std::optional<Blob> serverKey = std::nullopt);
};

/** \brief Equality operator for peer entries
 *
 * \sa PeerEntry
 */
bool operator==(const PeerEntry&, const PeerEntry&);

/** \brief Output a PeerEntry to stream
 *
 * \param os the output stream
 * \param entry the peer entry to output
 *
 * \return parameter \p os
 */
std::ostream& operator<<(std::ostream& os, const PeerEntry& entry);

}
}

#endif // CARDSERVER_PEERENTRY_HH_
