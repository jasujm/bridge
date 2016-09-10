#include "messaging/PeerEntryJsonSerializer.hh"

#include "cardserver/PeerEntry.hh"
#include "messaging/JsonSerializerUtility.hh"

namespace Bridge {
namespace Messaging {

using CardServer::PeerEntry;
using nlohmann::json;

const std::string IDENTITY_KEY {"id"};
const std::string ENDPOINT_KEY {"endpoint"};

json JsonConverter<PeerEntry>::convertToJson(const PeerEntry& entry)
{
    auto j = json {
        {
            IDENTITY_KEY,
            entry.identity
        }
    };
    optionalPut(j, ENDPOINT_KEY, entry.endpoint);
    return j;
}

PeerEntry JsonConverter<PeerEntry>::convertFromJson(const json& j)
{
    return PeerEntry {
        checkedGet<std::string>(j, IDENTITY_KEY),
        optionalGet<std::string>(j, ENDPOINT_KEY)};
}

}
}
