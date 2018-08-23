#include "messaging/PositionJsonSerializer.hh"

#include "bridge/Position.hh"
#include "messaging/JsonSerializerUtility.hh"

using nlohmann::json;

namespace Bridge {

void to_json(json& j, const Position position)
{
    j = Messaging::enumToJson(position, POSITION_TO_STRING_MAP.left);
}

void from_json(const json& j, Position& position)
{
    position = Messaging::jsonToEnum<Position>(j, POSITION_TO_STRING_MAP.right);
}

}
