#include "messaging/PositionJsonSerializer.hh"

#include "bridge/Position.hh"
#include "messaging/JsonSerializerUtility.hh"

using nlohmann::json;

namespace Bridge {
namespace Messaging {

json JsonConverter<Position>::convertToJson(const Position position)
{
    return enumToJson(position, POSITION_TO_STRING_MAP.left);
}

Position JsonConverter<Position>::convertFromJson(const json& j)
{
    return jsonToEnum<Position>(j, POSITION_TO_STRING_MAP.right);
}

}
}
