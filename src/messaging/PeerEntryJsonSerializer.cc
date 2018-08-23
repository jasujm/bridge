#include "messaging/PeerEntryJsonSerializer.hh"

#include "cardserver/PeerEntry.hh"
#include "messaging/JsonSerializerUtility.hh"

using nlohmann::json;

namespace Bridge {
namespace CardServer {

const std::string IDENTITY_KEY {"id"};
const std::string ENDPOINT_KEY {"endpoint"};

void to_json(json& j, const PeerEntry& entry)
{
    j[IDENTITY_KEY] = Messaging::blobToJson(entry.identity);
    j[ENDPOINT_KEY] = entry.endpoint;
}

void from_json(const json& j, PeerEntry& entry)
{
    entry.identity = Messaging::jsonToBlob(j.at(IDENTITY_KEY));
    entry.endpoint = j.at(ENDPOINT_KEY).get<decltype(entry.endpoint)>();
}

}
}
