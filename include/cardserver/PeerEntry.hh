/** \file
 *
 * \brief Definition of Bridge::CardServer::PeerEntry
 */

#ifndef CARDSERVER_PEERENTRY_HH_
#define CARDSERVER_PEERENTRY_HH_

#include <boost/operators.hpp>

#include <iosfwd>
#include <optional>
#include <string>

namespace Bridge {
namespace CardServer {

/** \brief Peer entry in card server protocol
 *
 * This struct is the internal representation of a peer of card server. The
 * init command in card server protocol consumes a list of peer entries.
 *
 * \sa \ref cardserverprotocol
 */
struct PeerEntry : private boost::equality_comparable<PeerEntry> {
    std::string identity;                   ///< \brief Peer identity
    std::optional<std::string> endpoint;  ///< \brief Card server endpoint

    /** \brief Create new peer entry
     *
     * \param identity see \ref identity
     * \param endpoint see \ref endpoint
     */
    explicit PeerEntry(
        std::string identity,
        std::optional<std::string> endpoint = std::nullopt);
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
