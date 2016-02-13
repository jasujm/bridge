#include "messaging/DealStateJsonSerializer.hh"

#include "messaging/CallJsonSerializer.hh"
#include "messaging/CardTypeJsonSerializer.hh"
#include "messaging/ContractJsonSerializer.hh"
#include "messaging/JsonSerializerUtility.hh"
#include "messaging/PositionJsonSerializer.hh"
#include "messaging/TricksWonJsonSerializer.hh"
#include "messaging/VulnerabilityJsonSerializer.hh"

#include <boost/optional/optional.hpp>
#include <boost/iterator/transform_iterator.hpp>

#include <vector>

using nlohmann::json;

namespace Bridge {
namespace Messaging {

const std::string DEAL_STATE_STAGE_KEY {"stage"};
const std::string DEAL_STATE_POSITION_IN_TURN_KEY {"positionInTurn"};
const std::string DEAL_STATE_VULNERABILITY_KEY {"vulnerability"};
const std::string DEAL_STATE_CARDS_KEY {"cards"};
const std::string DEAL_STATE_CALLS_KEY {"calls"};
const std::string DEAL_STATE_POSITION_KEY {"position"};
const std::string DEAL_STATE_CALL_KEY {"call"};
const std::string DEAL_STATE_DECLARER_KEY {"declarer"};
const std::string DEAL_STATE_CONTRACT_KEY {"contract"};
const std::string DEAL_STATE_CURRENT_TRICK_KEY {"currentTrick"};
const std::string DEAL_STATE_CARD_KEY {"card"};
const std::string DEAL_STATE_TRICKS_WON_KEY {"tricksWon"};

json JsonConverter<Stage>::convertToJson(const Stage stage)
{
    return enumToJson(stage, STAGE_TO_STRING_MAP.left);
}

Stage JsonConverter<Stage>::convertFromJson(const json& j)
{
    return jsonToEnum<Stage>(j, STAGE_TO_STRING_MAP.right);
}

json JsonConverter<DealState::Cards>::convertToJson(
    const DealState::Cards& cards)
{
    auto ret = json::object();
    for (const auto& p : cards) {
        auto arr = json::array();
        for (const auto& c : p.second) {
            arr.push_back(toJson(c));
        }
        ret[POSITION_TO_STRING_MAP.left.at(p.first)] = std::move(arr);
    }
    return ret;
}

DealState::Cards JsonConverter<DealState::Cards>::convertFromJson(const json& j)
{
    auto ret = DealState::Cards {};
    for (auto iter = j.begin(); iter != j.end(); ++iter) {
        std::vector<CardType> cards;
        for (const auto c : iter.value()) {
            cards.push_back(fromJson<CardType>(c));
        }
        ret.emplace(
            POSITION_TO_STRING_MAP.right.at(iter.key()), std::move(cards));
    }
    return ret;
}

json JsonConverter<DealState::Calls>::convertToJson(
    const DealState::Calls& calls)
{
    auto ret = json::array();
    for (const auto& c : calls) {
        ret.push_back(
            pairToJson(c, DEAL_STATE_POSITION_KEY, DEAL_STATE_CALL_KEY));
    }
    return ret;
}

DealState::Calls JsonConverter<DealState::Calls>::convertFromJson(const json& j)
{
    auto ret = DealState::Calls {};
    for (const auto c : j) {
        ret.emplace_back(
            jsonToPair<Position, Call>(
                c, DEAL_STATE_POSITION_KEY, DEAL_STATE_CALL_KEY));
    }
    return ret;
}

json JsonConverter<DealState::Trick>::convertToJson(
    const DealState::Trick& trick)
{
    auto ret = json::array();
    for (const auto& t : trick) {
        ret.push_back(
            pairToJson(t, DEAL_STATE_POSITION_KEY, DEAL_STATE_CARD_KEY));
    }
    return ret;
}

DealState::Trick JsonConverter<DealState::Trick>::convertFromJson(const json& j)
{
    auto ret = DealState::Trick {};
    for (const auto c : j) {
        ret.emplace_back(
            jsonToPair<Position, CardType>(
                c, DEAL_STATE_POSITION_KEY, DEAL_STATE_CARD_KEY));
    }
    return ret;
}

json JsonConverter<DealState>::convertToJson(const DealState& dealState)
{
    auto j = json {{DEAL_STATE_STAGE_KEY, toJson(dealState.stage)}};
    optionalPut(j, DEAL_STATE_POSITION_IN_TURN_KEY, dealState.positionInTurn);
    optionalPut(j, DEAL_STATE_VULNERABILITY_KEY, dealState.vulnerability);
    optionalPut(j, DEAL_STATE_CARDS_KEY, dealState.cards);
    optionalPut(j, DEAL_STATE_CALLS_KEY, dealState.calls);
    optionalPut(j, DEAL_STATE_DECLARER_KEY, dealState.declarer);
    optionalPut(j, DEAL_STATE_CONTRACT_KEY, dealState.contract);
    optionalPut(j, DEAL_STATE_CURRENT_TRICK_KEY, dealState.currentTrick);
    optionalPut(j, DEAL_STATE_TRICKS_WON_KEY, dealState.tricksWon);
    return j;
}

DealState JsonConverter<DealState>::convertFromJson(const json& j)
{
    auto state = DealState {};
    state.stage = checkedGet<Stage>(j, DEAL_STATE_STAGE_KEY);
    state.positionInTurn = optionalGet<Position>(
        j, DEAL_STATE_POSITION_IN_TURN_KEY);
    state.vulnerability = optionalGet<Vulnerability>(
        j, DEAL_STATE_VULNERABILITY_KEY);
    state.cards = optionalGet<DealState::Cards>(j, DEAL_STATE_CARDS_KEY);
    state.calls = optionalGet<DealState::Calls>(j, DEAL_STATE_CALLS_KEY);
    state.declarer = optionalGet<Position>(j, DEAL_STATE_DECLARER_KEY);
    state.contract = optionalGet<Contract>(j, DEAL_STATE_CONTRACT_KEY);
    state.currentTrick = optionalGet<DealState::Trick>(
        j, DEAL_STATE_CURRENT_TRICK_KEY);
    state.tricksWon = optionalGet<TricksWon>(j, DEAL_STATE_TRICKS_WON_KEY);
    return state;
}

}
}
