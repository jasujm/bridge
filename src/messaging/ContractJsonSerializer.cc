#include "messaging/ContractJsonSerializer.hh"

#include "bridge/Contract.hh"
#include "messaging/BidJsonSerializer.hh"
#include "messaging/JsonSerializerUtility.hh"

using nlohmann::json;

namespace Bridge {
namespace Messaging {

const std::string CONTRACT_BID_KEY {"bid"};
const std::string CONTRACT_DOUBLING_KEY {"doubling"};

template<>
json toJson(const Doubling& doubling)
{
    return enumToJson(doubling, DOUBLING_TO_STRING_MAP.left);
}

template<>
Doubling fromJson(const json& j)
{
    return jsonToEnum<Doubling>(j, DOUBLING_TO_STRING_MAP.right);
}

template<>
json toJson(const Contract& contract)
{
    return {
        {CONTRACT_BID_KEY, toJson(contract.bid)},
        {CONTRACT_DOUBLING_KEY, toJson(contract.doubling)}};
}

template<>
Contract fromJson(const json& j)
{
    return {
        checkedGet<Bid>(j, CONTRACT_BID_KEY),
        checkedGet<Doubling>(j, CONTRACT_DOUBLING_KEY)};
}

}
}
