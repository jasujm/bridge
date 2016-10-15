#include "main/GetMessageHandler.hh"

#include "bridge/AllowedCalls.hh"
#include "bridge/AllowedCards.hh"
#include "bridge/Call.hh"
#include "bridge/CardType.hh"
#include "bridge/Position.hh"
#include "bridge/Trick.hh"
#include "engine/BridgeEngine.hh"
#include "engine/DuplicateGameManager.hh"
#include "main/Commands.hh"
#include "main/PeerClientControl.hh"
#include "messaging/CallJsonSerializer.hh"
#include "messaging/CardTypeJsonSerializer.hh"
#include "messaging/DuplicateScoreSheetJsonSerializer.hh"
#include "messaging/JsonSerializer.hh"
#include "messaging/JsonSerializerUtility.hh"
#include "messaging/PositionJsonSerializer.hh"
#include "messaging/SerializationUtility.hh"
#include "Utility.hh"

#include <boost/optional/optional.hpp>
#include <boost/iterator/transform_iterator.hpp>

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
                Messaging::pairToJson<Position, Call>(
                    pair, POSITION_COMMAND, CALL_COMMAND));
        }
    }
    return calls.dump();
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

auto getCards(const Player& player, const BridgeEngine& engine)
{
    using CardTypeVector = std::vector<boost::optional<CardType>>;
    auto cards = nlohmann::json::object();
    for (const auto position : POSITIONS) {
        if (const auto hand = engine.getHand(engine.getPlayer(position))) {
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

auto getCurrentTrick(const BridgeEngine& engine)
{
    auto cards = nlohmann::json::object();
    if (const auto trick = engine.getCurrentTrick()) {
        for (const auto p : *trick) {
            const auto position = dereference(engine.getPosition(p.first));
            const auto& position_str = POSITION_TO_STRING_MAP.left.at(position);
            cards[position_str] = toJson(dereference(p.second.getType()));
        }
    }
    return cards.dump();
}

std::string getScore(const DuplicateGameManager& gameManager)
{
    return JsonSerializer::serialize(gameManager.getScoreSheet());
}

}

GetMessageHandler::GetMessageHandler(
    std::shared_ptr<const DuplicateGameManager> gameManager,
    std::shared_ptr<const BridgeEngine> engine,
    std::shared_ptr<const PeerClientControl> peerClientControl) :
    gameManager {std::move(gameManager)},
    engine {std::move(engine)},
    peerClientControl {std::move(peerClientControl)}
{
}

bool GetMessageHandler::doHandle(
    const std::string& identity, const ParameterVector& params,
    OutputSink sink)
{
    const auto keys = Messaging::deserializeParam<std::vector<std::string>>(
        JsonSerializer {}, params.begin(), params.end(), KEYS_COMMAND);
    const auto player = dereference(peerClientControl).getPlayer(identity);
    if (keys && player) {
        for (const auto& key : *keys) {
            if (internalContainsKey(key, ALLOWED_CALLS_COMMAND, sink)) {
                sink(getAllowedCalls(dereference(engine), *player));
            } else if (internalContainsKey(key, CALLS_COMMAND, sink)) {
                sink(getCalls(dereference(engine)));
            } else if (internalContainsKey(key, ALLOWED_CARDS_COMMAND, sink)) {
                sink(getAllowedCards(dereference(engine), *player));
            } else if (internalContainsKey(key, CARDS_COMMAND, sink)) {
                sink(getCards(*player, dereference(engine)));
            } else if (internalContainsKey(key, CURRENT_TRICK_COMMAND, sink)) {
                sink(getCurrentTrick(dereference(engine)));
            } else if (internalContainsKey(key, SCORE_COMMAND, sink)) {
                sink(getScore(dereference(gameManager)));
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
