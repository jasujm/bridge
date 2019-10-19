#include "main/GetMessageHandler.hh"
#include "bridge/AllowedCalls.hh"
#include "bridge/AllowedCards.hh"
#include "bridge/Call.hh"
#include "bridge/CardType.hh"
#include "bridge/Contract.hh"
#include "bridge/Position.hh"
#include "bridge/Trick.hh"
#include "bridge/TricksWon.hh"
#include "bridge/Vulnerability.hh"
#include "engine/BridgeEngine.hh"
#include "engine/DuplicateGameManager.hh"
#include "main/Commands.hh"
#include "main/BridgeGameInfo.hh"
#include "main/NodePlayerControl.hh"
#include "messaging/CallJsonSerializer.hh"
#include "messaging/CardTypeJsonSerializer.hh"
#include "messaging/ContractJsonSerializer.hh"
#include "messaging/JsonSerializer.hh"
#include "messaging/JsonSerializerUtility.hh"
#include "messaging/PositionJsonSerializer.hh"
#include "messaging/Replies.hh"
#include "messaging/SerializationUtility.hh"
#include "messaging/TricksWonJsonSerializer.hh"
#include "messaging/UuidJsonSerializer.hh"
#include "messaging/VulnerabilityJsonSerializer.hh"
#include "Utility.hh"

#include <boost/logic/tribool.hpp>

#include <algorithm>
#include <optional>
#include <iterator>
#include <string>
#include <utility>
#include <vector>

namespace Bridge {
namespace Main {

namespace {

using Engine::BridgeEngine;
using Engine::DuplicateGameManager;
using Messaging::JsonSerializer;

std::string getPosition(const BridgeEngine& engine, const Player& player)
{
    return JsonSerializer::serialize(engine.getPosition(player));
}

std::string getPositionInTurn(const BridgeEngine& engine)
{
    return JsonSerializer::serialize(engine.getPositionInTurn());
}

std::string getAllowedCalls(const BridgeEngine& engine, const Player& player)
{
    auto allowed_calls = std::vector<Call> {};
    if (engine.getPlayerInTurn() == &player) {
        if (const auto bidding = engine.getBidding()) {
            getAllowedCalls(*bidding, std::back_inserter(allowed_calls));
        }
    }
    return JsonSerializer::serialize(allowed_calls);
}

std::string getCalls(const BridgeEngine& engine)
{
    auto calls = nlohmann::json::array();
    if (const auto& bidding = engine.getBidding()) {
        for (const auto& [ position, call ] : *bidding) {
            calls.push_back(
                nlohmann::json {
                    { POSITION_COMMAND, position },
                    { CALL_COMMAND, call }
                });
        }
    }
    return calls.dump();
}

template<typename Result>
std::string getBiddingResult(
    const BridgeEngine& engine,
    std::optional<std::optional<Result>> (Bidding::*getResult)() const)
{
    auto result = std::optional<Result> {};
    if (const auto bidding = engine.getBidding()) {
        if (bidding->hasContract()) {
            result = dereference((bidding->*getResult)());
        }
    }
    return JsonSerializer::serialize(result);
}

std::string getAllowedCards(const BridgeEngine& engine, const Player& player)
{
    auto allowed_cards = std::vector<CardType> {};
    if (engine.getPlayerInTurn() == &player) {
        if (const auto trick = engine.getCurrentTrick()) {
            getAllowedCards(*trick, std::back_inserter(allowed_cards));
        }
    }
    return JsonSerializer::serialize(allowed_cards);
}

std::string getCards(const Player& player, const BridgeEngine& engine)
{
    using CardTypeVector = std::vector<std::optional<CardType>>;
    auto cards = nlohmann::json::object();
    for (const auto position : POSITIONS) {
        if (const auto hand = engine.getHand(position)) {
            auto type_func = [](const auto& card) { return card.getType(); };
            const auto cards_in_hand = engine.isVisible(*hand, player) ?
                CardTypeVector(
                    boost::make_transform_iterator(hand->begin(), type_func),
                    boost::make_transform_iterator(hand->end(), type_func)) :
                CardTypeVector(
                    std::distance(hand->begin(), hand->end()), std::nullopt);
            const auto& position_str = POSITION_TO_STRING_MAP.left.at(position);
            cards[position_str] = cards_in_hand;
        }
    }
    return cards.dump();
}

std::string getTrick(const BridgeEngine& engine)
{
    auto cards = nlohmann::json::array();
    if (const auto trick = engine.getCurrentTrick()) {
        for (const auto pair : *trick) {
            const auto position = engine.getPosition(pair.first);
            const auto card = pair.second.getType();
            cards.push_back(
                nlohmann::json {
                    { POSITION_COMMAND, position },
                    { CARD_COMMAND, card },
                });
        }
    }
    return cards.dump();
}

std::string getTricksWon(const BridgeEngine& engine)
{
    return JsonSerializer::serialize(
        engine.getTricksWon().value_or(TricksWon {0, 0}));
}

std::string getVulnerability(const DuplicateGameManager& gameManager)
{
    return JsonSerializer::serialize(gameManager.getVulnerability());
}

}

GetMessageHandler::GetMessageHandler(
    std::weak_ptr<const BridgeGameInfo> game,
    std::shared_ptr<const NodePlayerControl> nodePlayerControl) :
    game {std::move(game)},
    nodePlayerControl {std::move(nodePlayerControl)}
{
}

void GetMessageHandler::doHandle(
    ExecutionContext, const Messaging::Identity& identity,
    const ParameterVector& params, Messaging::Response& response)
{
    auto keys = Messaging::deserializeParam<std::vector<std::string>>(
        JsonSerializer {}, params.begin(), params.end(), KEYS_COMMAND);
    const auto game_ = game.lock();
    const auto player = dereference(nodePlayerControl).getPlayer(identity);
    if (game_ && player) {
        if (keys) {
            // keys will be binary searched below
            std::sort(keys->begin(), keys->end());
        }
        const auto& engine = game_->getEngine();
        auto add_response_for_key = [this, &keys](
            const auto& key, auto& response)
        {
            // keys are sorted above
            if (!keys || std::binary_search(keys->begin(), keys->end(), key)) {
                response.addFrame(asBytes(key));
                return true;
            }
            return false;
        };
        if (add_response_for_key(POSITION_COMMAND, response)) {
            response.addFrame(asBytes(getPosition(engine, *player)));
        }
        if (add_response_for_key(POSITION_IN_TURN_COMMAND, response)) {
            response.addFrame(asBytes(getPositionInTurn(engine)));
        }
        if (add_response_for_key(DECLARER_COMMAND, response)) {
            response.addFrame(
                asBytes(
                    getBiddingResult(engine, &Bidding::getDeclarerPosition)));
        }
        if (add_response_for_key(CONTRACT_COMMAND, response)) {
            response.addFrame(
                asBytes(getBiddingResult(engine, &Bidding::getContract)));
        }
        if (add_response_for_key(ALLOWED_CALLS_COMMAND, response)) {
            response.addFrame(asBytes(getAllowedCalls(engine, *player)));
        }
        if (add_response_for_key(CALLS_COMMAND, response)) {
            response.addFrame(asBytes(getCalls(engine)));
        }
        if (add_response_for_key(ALLOWED_CARDS_COMMAND, response)) {
            response.addFrame(asBytes(getAllowedCards(engine, *player)));
        }
        if (add_response_for_key(CARDS_COMMAND, response)) {
            response.addFrame(asBytes(getCards(*player, engine)));
        }
        if (add_response_for_key(TRICK_COMMAND, response)) {
            response.addFrame(asBytes(getTrick(engine)));
        }
        if (add_response_for_key(TRICKS_WON_COMMAND, response)) {
            response.addFrame(asBytes(getTricksWon(engine)));
        }
        if (add_response_for_key(VULNERABILITY_COMMAND, response)) {
            response.addFrame(
                asBytes(getVulnerability(game_->getGameManager())));
        }
        response.setStatus(Messaging::REPLY_SUCCESS);
    } else {
        response.setStatus(Messaging::REPLY_FAILURE);
    }
}

}
}
