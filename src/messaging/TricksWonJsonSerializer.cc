#include "messaging/TricksWonJsonSerializer.hh"

#include "bridge/Partnership.hh"
#include "bridge/TricksWon.hh"
#include "messaging/JsonSerializerUtility.hh"

using nlohmann::json;

namespace Bridge {

void to_json(nlohmann::json& j, const TricksWon& tricksWon)
{
    j[PARTNERSHIP_TO_STRING_MAP.left.at(Partnership::NORTH_SOUTH)] =
        tricksWon.tricksWonByNorthSouth;
    j[PARTNERSHIP_TO_STRING_MAP.left.at(Partnership::EAST_WEST)] =
        tricksWon.tricksWonByEastWest;
}

void from_json(const nlohmann::json& j, TricksWon& tricksWon)
{
    tricksWon.tricksWonByNorthSouth =
        j.at(PARTNERSHIP_TO_STRING_MAP.left.at(Partnership::NORTH_SOUTH));
    tricksWon.tricksWonByEastWest =
        j.at(PARTNERSHIP_TO_STRING_MAP.left.at(Partnership::EAST_WEST));
}

}
