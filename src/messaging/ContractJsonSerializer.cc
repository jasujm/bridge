#include "messaging/ContractJsonSerializer.hh"

#include "bridge/Contract.hh"
#include "messaging/BidJsonSerializer.hh"
#include "messaging/JsonSerializerUtility.hh"

using nlohmann::json;

namespace Bridge {

const std::string CONTRACT_BID_KEY {"bid"};
const std::string CONTRACT_DOUBLING_KEY {"doubling"};

void to_json(json& j, const Doubling doubling)
{
    j = Messaging::enumToJson(doubling, DOUBLING_TO_STRING_MAP.left);
}

void from_json(const json& j, Doubling& doubling)
{
    doubling = Messaging::jsonToEnum<Doubling>(j, DOUBLING_TO_STRING_MAP.right);
}

void to_json(json& j, const Contract& contract)
{
    j[CONTRACT_BID_KEY] = contract.bid;
    j[CONTRACT_DOUBLING_KEY] = contract.doubling;
}

void from_json(const json& j, Contract& contract)
{
    contract.bid = j.at(CONTRACT_BID_KEY);
    contract.doubling = j.at(CONTRACT_DOUBLING_KEY);
}

}
