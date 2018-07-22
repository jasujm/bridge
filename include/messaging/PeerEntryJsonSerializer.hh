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
 * - &lt;endpoint&gt; is a string containing the endpoint of the peer. Optional.
 *
 * \sa \ref cardserverprotocol
 */

#ifndef MESSAGING_PEERENTRYJSONSERIALIZER_HH_
#define MESSAGING_PEERENTRYJSONSERIALIZER_HH_

#include "messaging/JsonSerializer.hh"

namespace Bridge {

namespace CardServer {
struct PeerEntry;
}

namespace Messaging {

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

/** \brief JSON converter for CardServer::PeerEntry
 *
 * \sa JsonSerializer.hh, \ref jsonpeerentry
 */
template<>
struct JsonConverter<CardServer::PeerEntry>
{
    /** \brief Convert CardServer::PeerEntry to JSON
     */
    static nlohmann::json convertToJson(const CardServer::PeerEntry& peerEntry);

    /** \brief Convert JSON to CardServer::PeerEntry
     */
    static CardServer::PeerEntry convertFromJson(const nlohmann::json& j);
};

}
}

#endif // MESSAGING_PEERENTRYJSONSERIALIZER_HH_
