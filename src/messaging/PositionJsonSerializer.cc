#include "messaging/PositionJsonSerializer.hh"

#include "bridge/Position.hh"
#include "messaging/JsonSerializerUtility.hh"

using nlohmann::json;

namespace Bridge {
namespace Messaging {

template<>
json toJson(const Position& position)
{
    return enumToJson(position, POSITION_TO_STRING_MAP.left);
}

template<>
Position fromJson(const json& j)
{
    return jsonToEnum<Position>(j, POSITION_TO_STRING_MAP.right);
}

}
}
