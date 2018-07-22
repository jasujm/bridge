#include "messaging/JsonSerializerUtility.hh"

#include "messaging/SerializationFailureException.hh"
#include "HexUtility.hh"

#include <iterator>

namespace Bridge {
namespace Messaging {

using nlohmann::json;

json JsonConverter<Bridge::Blob>::convertToJson(const Blob& blob)
{
    auto ret = std::string {};
    ret.reserve(2 * blob.size());
    toHex(blob.begin(), blob.end(), std::back_inserter(ret));
    return ret;
}

Bridge::Blob JsonConverter<Bridge::Blob>::convertFromJson(const json& j)
{
    if (!j.is_string()) {
        throw SerializationFailureException {};
    }
    const auto& encoded_blob = j.get_ref<const json::string_t&>();
    if (!isValidHex(encoded_blob.begin(), encoded_blob.end())) {
        throw SerializationFailureException {};
    }
    auto ret = Blob {};
    ret.reserve(encoded_blob.size() / 2);
    fromHex(encoded_blob.begin(), encoded_blob.end(), std::back_inserter(ret));
    return ret;
}

}
}
