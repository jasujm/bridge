#include "messaging/PartnershipJsonSerializer.hh"

#include "bridge/Partnership.hh"
#include "messaging/JsonSerializerUtility.hh"

using nlohmann::json;

namespace Bridge {
namespace Messaging {

template<>
json toJson(const Partnership& partnership)
{
    return enumToJson(partnership, PARTNERSHIP_TO_STRING_MAP.left);
}

template<>
Partnership fromJson(const json& j)
{
    return jsonToEnum<Partnership>(j, PARTNERSHIP_TO_STRING_MAP.right);
}

}
}
