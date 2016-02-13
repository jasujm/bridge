#include "messaging/PartnershipJsonSerializer.hh"

#include "bridge/Partnership.hh"
#include "messaging/JsonSerializerUtility.hh"

using nlohmann::json;

namespace Bridge {
namespace Messaging {

json JsonConverter<Partnership>::convertToJson(const Partnership partnership)
{
    return enumToJson(partnership, PARTNERSHIP_TO_STRING_MAP.left);
}

Partnership JsonConverter<Partnership>::convertFromJson(const json& j)
{
    return jsonToEnum<Partnership>(j, PARTNERSHIP_TO_STRING_MAP.right);
}

}
}
