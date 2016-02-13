#include "messaging/TricksWonJsonSerializer.hh"

#include "bridge/Partnership.hh"
#include "bridge/TricksWon.hh"
#include "messaging/JsonSerializerUtility.hh"

using nlohmann::json;

namespace Bridge {
namespace Messaging {

nlohmann::json JsonConverter<TricksWon>::convertToJson(
    const TricksWon& tricksWon)
{
    return {
        {
            PARTNERSHIP_TO_STRING_MAP.left.at(Partnership::NORTH_SOUTH),
            tricksWon.tricksWonByNorthSouth
        },
        {
            PARTNERSHIP_TO_STRING_MAP.left.at(Partnership::EAST_WEST),
            tricksWon.tricksWonByEastWest
        }
    };
}

TricksWon JsonConverter<TricksWon>::convertFromJson(const nlohmann::json& j)
{
    return {
        checkedGet<int>(
            j, PARTNERSHIP_TO_STRING_MAP.left.at(Partnership::NORTH_SOUTH)),
        checkedGet<int>(
            j, PARTNERSHIP_TO_STRING_MAP.left.at(Partnership::EAST_WEST))};
}

}
}
