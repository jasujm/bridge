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

#include <boost/iterator/transform_iterator.hpp>
#include <boost/logic/tribool.hpp>

#include <algorithm>
#include <map>
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

const auto ALL_KEYS = std::vector<std::string> {
    POSITION_COMMAND, POSITION_IN_TURN_COMMAND, ALLOWED_CALLS_COMMAND,
    CALLS_COMMAND, DECLARER_COMMAND, CONTRACT_COMMAND, ALLOWED_CARDS_COMMAND,
    CARDS_COMMAND, TRICK_COMMAND, TRICKS_WON_COMMAND, VULNERABILITY_COMMAND
};

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

std::vector<std::string> GetMessageHandler::getAllKeys()
{
    return ALL_KEYS;
}

void GetMessageHandler::doHandle(
    Messaging::SynchronousExecutionPolicy&, const Messaging::Identity& identity,
    const ParameterVector& params, Messaging::Response& response)
{
    const auto keys_param = Messaging::deserializeParam<std::vector<std::string>>(
        JsonSerializer {}, params.begin(), params.end(), KEYS_COMMAND);
    const auto& keys = bool(keys_param) ? *keys_param : ALL_KEYS;
    const auto game_ = game.lock();
    const auto player = dereference(nodePlayerControl).getPlayer(identity);
    if (game_ && player) {
        const auto& engine = game_->getEngine();
        for (const auto& key : keys) {
            if (internalContainsKey(key, POSITION_COMMAND, response)) {
                response.addFrame(asBytes(getPosition(engine, *player)));
            } else if (internalContainsKey(key, POSITION_IN_TURN_COMMAND, response)) {
                response.addFrame(asBytes(getPositionInTurn(engine)));
            } else if (internalContainsKey(key, DECLARER_COMMAND, response)) {
                response.addFrame(asBytes(getBiddingResult(engine, &Bidding::getDeclarerPosition)));
            } else if (internalContainsKey(key, CONTRACT_COMMAND, response)) {
                response.addFrame(asBytes(getBiddingResult(engine, &Bidding::getContract)));
            } else if (internalContainsKey(key, ALLOWED_CALLS_COMMAND, response)) {
                response.addFrame(asBytes(getAllowedCalls(engine, *player)));
            } else if (internalContainsKey(key, CALLS_COMMAND, response)) {
                response.addFrame(asBytes(getCalls(engine)));
            } else if (internalContainsKey(key, ALLOWED_CARDS_COMMAND, response)) {
                response.addFrame(asBytes(getAllowedCards(engine, *player)));
            } else if (internalContainsKey(key, CARDS_COMMAND, response)) {
                response.addFrame(asBytes(getCards(*player, engine)));
            } else if (internalContainsKey(key, TRICK_COMMAND, response)) {
                response.addFrame(asBytes(getTrick(engine)));
            } else if (internalContainsKey(key, TRICKS_WON_COMMAND, response)) {
                response.addFrame(asBytes(getTricksWon(engine)));
            } else if (internalContainsKey(key, VULNERABILITY_COMMAND, response)) {
                response.addFrame(asBytes(getVulnerability(game_->getGameManager())));
            }
        }
        response.setStatus(Messaging::REPLY_SUCCESS);
    } else {
        response.setStatus(Messaging::REPLY_FAILURE);
    }
}

bool GetMessageHandler::internalContainsKey(
    const std::string& key, const std::string& expected,
    Messaging::Response& response) const
{
    if (key == expected) {
        response.addFrame(asBytes(key));
        return true;
    }
    return false;
}

}
}
