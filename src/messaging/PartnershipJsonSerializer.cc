#include "messaging/PartnershipJsonSerializer.hh"

#include "bridge/Partnership.hh"
#include "messaging/JsonSerializerUtility.hh"

using nlohmann::json;

namespace Bridge {

void to_json(json& j, const Partnership partnership)
{
    j = Messaging::enumToJson(partnership, PARTNERSHIP_TO_STRING_MAP.left);
}

void from_json(const json& j, Partnership& partnership)
{
    partnership = Messaging::jsonToEnum<Partnership>(
        j, PARTNERSHIP_TO_STRING_MAP.right);
}

}
