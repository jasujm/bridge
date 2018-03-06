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
#include "messaging/DuplicateScoreSheetJsonSerializer.hh"
#include "messaging/JsonSerializer.hh"
#include "messaging/JsonSerializerUtility.hh"
#include "messaging/PositionJsonSerializer.hh"
#include "messaging/SerializationUtility.hh"
#include "messaging/TricksWonJsonSerializer.hh"
#include "messaging/UuidJsonSerializer.hh"
#include "messaging/VulnerabilityJsonSerializer.hh"
#include "Utility.hh"

#include <boost/iterator/transform_iterator.hpp>
#include <boost/optional/optional.hpp>
#include <boost/logic/tribool.hpp>

#include <algorithm>
#include <map>
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
using Messaging::toJson;

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
        for (const auto& pair : *bidding) {
            calls.push_back(
                Messaging::pairToJson(pair, POSITION_COMMAND, CALL_COMMAND));
        }
    }
    return calls.dump();
}

template<typename Result>
std::string getBiddingResult(
    const BridgeEngine& engine,
    boost::optional<boost::optional<Result>> (Bidding::*getResult)() const)
{
    auto result = boost::optional<Result> {};
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
    using CardTypeVector = std::vector<boost::optional<CardType>>;
    auto cards = nlohmann::json::object();
    for (const auto position : POSITIONS) {
        if (const auto hand = engine.getHand(position)) {
            auto type_func = [](const auto& card) { return card.getType(); };
            const auto cards_in_hand = engine.isVisible(*hand, player) ?
                CardTypeVector(
                    boost::make_transform_iterator(hand->begin(), type_func),
                    boost::make_transform_iterator(hand->end(), type_func)) :
                CardTypeVector(
                    std::distance(hand->begin(), hand->end()), boost::none);
            const auto& position_str = POSITION_TO_STRING_MAP.left.at(position);
            cards[position_str] = toJson(cards_in_hand);
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
                Messaging::pairToJson(
                    std::make_pair(position, card),
                    POSITION_COMMAND, CARD_COMMAND));
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

std::string getScore(const DuplicateGameManager& gameManager)
{
    return JsonSerializer::serialize(gameManager.getScoreSheet());
}

}

GetMessageHandler::GetMessageHandler(
    GetGameFunction games,
    std::shared_ptr<const NodePlayerControl> nodePlayerControl) :
    games {std::move(games)},
    nodePlayerControl {std::move(nodePlayerControl)}
{
}

bool GetMessageHandler::doHandle(
    const std::string& identity, const ParameterVector& params,
    OutputSink sink)
{
    const auto game_uuid = Messaging::deserializeParam<Uuid>(
        JsonSerializer {}, params.begin(), params.end(), GAME_COMMAND);
    if (!game_uuid) {
        return false;
    }
    const auto game = games(*game_uuid);
    const auto keys = Messaging::deserializeParam<std::vector<std::string>>(
        JsonSerializer {}, params.begin(), params.end(), KEYS_COMMAND);
    const auto player = dereference(nodePlayerControl).getPlayer(identity);
    if (game && keys && player) {
        const auto& engine = game->getEngine();
        for (const auto& key : *keys) {
            if (internalContainsKey(key, POSITION_COMMAND, sink)) {
                sink(getPosition(engine, *player));
            } else if (internalContainsKey(key, POSITION_IN_TURN_COMMAND, sink)) {
                sink(getPositionInTurn(engine));
            } else if (internalContainsKey(key, DECLARER_COMMAND, sink)) {
                sink(getBiddingResult(engine, &Bidding::getDeclarerPosition));
            } else if (internalContainsKey(key, CONTRACT_COMMAND, sink)) {
                sink(getBiddingResult(engine, &Bidding::getContract));
            } else if (internalContainsKey(key, ALLOWED_CALLS_COMMAND, sink)) {
                sink(getAllowedCalls(engine, *player));
            } else if (internalContainsKey(key, CALLS_COMMAND, sink)) {
                sink(getCalls(engine));
            } else if (internalContainsKey(key, ALLOWED_CARDS_COMMAND, sink)) {
                sink(getAllowedCards(engine, *player));
            } else if (internalContainsKey(key, CARDS_COMMAND, sink)) {
                sink(getCards(*player, engine));
            } else if (internalContainsKey(key, TRICK_COMMAND, sink)) {
                sink(getTrick(engine));
            } else if (internalContainsKey(key, TRICKS_WON_COMMAND, sink)) {
                sink(getTricksWon(engine));
            } else if (internalContainsKey(key, VULNERABILITY_COMMAND, sink)) {
                sink(getVulnerability(game->getGameManager()));
            } else if (internalContainsKey(key, SCORE_COMMAND, sink)) {
                sink(getScore(game->getGameManager()));
            }
        }
        return true;
    }
    return false;
}

bool GetMessageHandler::internalContainsKey(
    const std::string& key, const std::string& expected, OutputSink& sink) const
{
    if (key == expected) {
        sink(key);
        return true;
    }
    return false;
}

}
}
