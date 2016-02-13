#include "messaging/BidJsonSerializer.hh"

#include "bridge/Bid.hh"
#include "messaging/JsonSerializerUtility.hh"

using nlohmann::json;

namespace Bridge {
namespace Messaging {

const std::string BID_LEVEL_KEY {"level"};
const std::string BID_STRAIN_KEY {"strain"};

json JsonConverter<Strain>::convertToJson(const Strain strain)
{
    return enumToJson(strain, STRAIN_TO_STRING_MAP.left);
}

Strain JsonConverter<Strain>::convertFromJson(const json& j)
{
    return jsonToEnum<Strain>(j, STRAIN_TO_STRING_MAP.right);
}

json JsonConverter<Bid>::convertToJson(const Bid& bid)
{
    return {
        {BID_LEVEL_KEY, json(bid.level)},
        {BID_STRAIN_KEY, toJson(bid.strain)}};
}

Bid JsonConverter<Bid>::convertFromJson(const json& j)
{
    return {
        validate(checkedGet<int>(j, BID_LEVEL_KEY), Bid::levelValid),
        checkedGet<Strain>(j, BID_STRAIN_KEY)};
}

}
}
