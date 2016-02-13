#include "messaging/ContractJsonSerializer.hh"

#include "bridge/Contract.hh"
#include "messaging/BidJsonSerializer.hh"
#include "messaging/JsonSerializerUtility.hh"

using nlohmann::json;

namespace Bridge {
namespace Messaging {

const std::string CONTRACT_BID_KEY {"bid"};
const std::string CONTRACT_DOUBLING_KEY {"doubling"};

json JsonConverter<Doubling>::convertToJson(const Doubling doubling)
{
    return enumToJson(doubling, DOUBLING_TO_STRING_MAP.left);
}

Doubling JsonConverter<Doubling>::convertFromJson(const json& j)
{
    return jsonToEnum<Doubling>(j, DOUBLING_TO_STRING_MAP.right);
}

json JsonConverter<Contract>::convertToJson(const Contract& contract)
{
    return {
        {CONTRACT_BID_KEY, toJson(contract.bid)},
        {CONTRACT_DOUBLING_KEY, toJson(contract.doubling)}};
}

Contract JsonConverter<Contract>::convertFromJson(const json& j)
{
    return {
        checkedGet<Bid>(j, CONTRACT_BID_KEY),
        checkedGet<Doubling>(j, CONTRACT_DOUBLING_KEY)};
}

}
}
