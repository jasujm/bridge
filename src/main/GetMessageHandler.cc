#include "main/GetMessageHandler.hh"

#include "bridge/AllowedCalls.hh"
#include "bridge/AllowedCards.hh"
#include "bridge/Call.hh"
#include "bridge/CardType.hh"
#include "engine/BridgeEngine.hh"
#include "engine/DuplicateGameManager.hh"
#include "main/Commands.hh"
#include "main/PeerClientControl.hh"
#include "messaging/CallJsonSerializer.hh"
#include "messaging/CardTypeJsonSerializer.hh"
#include "messaging/DuplicateScoreSheetJsonSerializer.hh"
#include "messaging/JsonSerializer.hh"
#include "messaging/JsonSerializerUtility.hh"
#include "messaging/SerializationUtility.hh"
#include "Utility.hh"

#include <boost/optional/optional.hpp>

#include <algorithm>
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
            } else if (internalContainsKey(key, ALLOWED_CARDS_COMMAND, sink)) {
                sink(getAllowedCards(dereference(engine), *player));
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
