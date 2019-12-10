#include "messaging/TricksWonJsonSerializer.hh"

#include "bridge/Partnership.hh"
#include "bridge/TricksWon.hh"
#include "messaging/JsonSerializerUtility.hh"

using nlohmann::json;

namespace Bridge {

void to_json(nlohmann::json& j, const TricksWon& tricksWon)
{
    j.emplace(Partnerships::NORTH_SOUTH_VALUE, tricksWon.tricksWonByNorthSouth);
    j.emplace(Partnerships::EAST_WEST_VALUE, tricksWon.tricksWonByEastWest);
}

void from_json(const nlohmann::json& j, TricksWon& tricksWon)
{
    tricksWon.tricksWonByNorthSouth = j.at(std::string {Partnerships::NORTH_SOUTH_VALUE});
    tricksWon.tricksWonByEastWest = j.at(std::string {Partnerships::EAST_WEST_VALUE});
}

}
