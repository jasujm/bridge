/** \file
 *
 * \brief Definition of JSON serializer for Bridge::CardServer::PeerEntry
 *
 * \page jsonpeerentry Peer entry JSON representation
 *
 * A Bridge::CardServer::PeerEntry is represented by a JSON object consisting
 * of the following
 *
 * \code{.json}
 * {
 *     { "id": <identity> },
 *     { "endpoint": <endpoint> }
 * }
 * \endcode
 *
 * - &lt;id&gt; is a opaque hex encoded binary string identifying the peer
 * - &lt;endpoint&gt; is a string containing the endpoint of the peer, or null
 *   if the peer will connect instead of binding
 *
 * \sa \ref cardserverprotocol
 */

#ifndef MESSAGING_PEERENTRYJSONSERIALIZER_HH_
#define MESSAGING_PEERENTRYJSONSERIALIZER_HH_

#include <json.hpp>

namespace Bridge {
namespace CardServer {

struct PeerEntry;

/** \brief Key for PeerEntry::identity
 *
 * \sa \ref jsonpeerentry
 */
extern const std::string IDENTITY_KEY;

/** \brief Key for PeerEntry::endpoint
 *
 * \sa \ref jsonpeerentry
 */
extern const std::string ENDPOINT_KEY;

/** \brief Convert CardServer::PeerEntry to JSON
 */
void to_json(nlohmann::json&, const PeerEntry&);

/** \brief Convert JSON to CardServer::PeerEntry
 */
void from_json(const nlohmann::json&, PeerEntry&);

}
}

#endif // MESSAGING_PEERENTRYJSONSERIALIZER_HH_
