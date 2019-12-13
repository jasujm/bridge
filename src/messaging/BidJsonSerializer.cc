#include "messaging/BidJsonSerializer.hh"

#include "bridge/Bid.hh"
#include "messaging/JsonSerializerUtility.hh"

using nlohmann::json;

namespace Bridge {

const std::string BID_LEVEL_KEY {"level"};
const std::string BID_STRAIN_KEY {"strain"};

void to_json(json& j, const Bid& bid)
{
    j.emplace(BID_LEVEL_KEY, bid.level);
    j.emplace(BID_STRAIN_KEY, bid.strain);
}

void from_json(const json& j, Bid& bid)
{
    bid.level = Messaging::validate(
        j.at(BID_LEVEL_KEY).get<int>(), Bid::levelValid);
    bid.strain = j.at(BID_STRAIN_KEY);
}

}
