#include "messaging/JsonSerializerUtility.hh"

#include "messaging/SerializationFailureException.hh"
#include "HexUtility.hh"

using nlohmann::json;

namespace nlohmann {

void adl_serializer<Bridge::Blob>::to_json(json& j, const Bridge::Blob& blob)
{
    j = Bridge::Messaging::blobToJson(blob);
}

void adl_serializer<Bridge::Blob>::from_json(const json& j, Bridge::Blob& blob)
{
    blob = Bridge::Messaging::jsonToBlob(j);
}

}

namespace Bridge {
namespace Messaging {

json blobToJson(const Blob& blob)
{
    return toHex(blob);
}

Blob jsonToBlob(const json& j)
{
    if (!j.is_string()) {
        throw SerializationFailureException {};
    }
    const auto& encoded_blob = j.get_ref<const json::string_t&>();
    if (!isValidHex(encoded_blob.begin(), encoded_blob.end())) {
        throw SerializationFailureException {};
    }
    return fromHex(encoded_blob);
}

}
}
