#include "messaging/PeerEntryJsonSerializer.hh"

#include "cardserver/PeerEntry.hh"
#include "messaging/JsonSerializerUtility.hh"

#include <type_traits>

using nlohmann::json;

namespace Bridge {
namespace CardServer {

const std::string IDENTITY_KEY {"id"};
const std::string ENDPOINT_KEY {"endpoint"};
const std::string SERVER_KEY_KEY {"serverKey"};

void to_json(json& j, const PeerEntry& entry)
{
    j[IDENTITY_KEY] = Messaging::blobToJson(entry.id);
    j[ENDPOINT_KEY] = entry.endpoint;
    if (entry.serverKey) {
        j[SERVER_KEY_KEY] = *entry.serverKey;
    }
}

void from_json(const json& j, PeerEntry& entry)
{
    entry.id = Messaging::jsonToBlob(j.at(IDENTITY_KEY));
    entry.endpoint = j.at(ENDPOINT_KEY).get<decltype(entry.endpoint)>();
    if (const auto iter = j.find(SERVER_KEY_KEY); iter != j.end()) {
        entry.serverKey =
            iter->get<std::remove_reference_t<decltype(*entry.serverKey)>>();
    }
}

}
}
