#include "messaging/JsonSerializerUtility.hh"

#include "messaging/SerializationFailureException.hh"
#include "HexUtility.hh"

using nlohmann::json;

namespace nlohmann {

void adl_serializer<Bridge::Blob>::to_json(json& j, const Bridge::Blob& blob)
{
    j = Bridge::toHex(blob);
}

void adl_serializer<Bridge::Blob>::from_json(const json& j, Bridge::Blob& blob)
{
    if (!j.is_string()) {
        throw Bridge::Messaging::SerializationFailureException {};
    }
    const auto& encoded_blob = j.get_ref<const json::string_t&>();
    if (!Bridge::isValidHex(encoded_blob.begin(), encoded_blob.end())) {
        throw Bridge::Messaging::SerializationFailureException {};
    }
    blob = Bridge::fromHex(encoded_blob);
}

}
