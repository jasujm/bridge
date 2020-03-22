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
#include "main/NodePlayerControl.hh"
#include "messaging/CallJsonSerializer.hh"
#include "messaging/CardTypeJsonSerializer.hh"
#include "messaging/ContractJsonSerializer.hh"
#include "messaging/JsonSerializer.hh"
#include "messaging/JsonSerializerUtility.hh"
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

auto getPosition(const BridgeEngine& engine, const Player& player)
{
    return engine.getPosition(player);
}

auto getPositionInTurn(const BridgeEngine& engine)
{
    return engine.getPositionInTurn();
}

auto getAllowedCalls(const BridgeEngine& engine, const Player& player)
{
    auto allowed_calls = std::vector<Call> {};
    if (engine.getPlayerInTurn() == &player) {
        if (const auto bidding = engine.getBidding()) {
            getAllowedCalls(*bidding, std::back_inserter(allowed_calls));
        }
    }
    return allowed_calls;
}

auto getCalls(const BridgeEngine& engine)
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
    return calls;
}

template<typename Result>
auto getBiddingResult(
    const BridgeEngine& engine,
    std::optional<std::optional<Result>> (Bidding::*getResult)() const)
{
    auto result = std::optional<Result> {};
    if (const auto bidding = engine.getBidding()) {
        if (bidding->hasContract()) {
            result = dereference((bidding->*getResult)());
        }
    }
    return result;
}

auto getAllowedCards(const BridgeEngine& engine, const Player& player)
{
    auto allowed_cards = std::vector<CardType> {};
    if (engine.getPlayerInTurn() == &player) {
        if (const auto trick = engine.getCurrentTrick()) {
            getAllowedCards(*trick, std::back_inserter(allowed_cards));
        }
    }
    return allowed_cards;
}

auto getCards(const Player& player, const BridgeEngine& engine)
{
    using CardTypeVector = std::vector<std::optional<CardType>>;
    auto cards = nlohmann::json::object();
    for (const auto position : Position::all()) {
        if (const auto hand = engine.getHand(position)) {
            auto type_func = [](const auto& card) { return card.getType(); };
            const auto cards_in_hand = engine.isVisible(*hand, player) ?
                CardTypeVector(
                    boost::make_transform_iterator(hand->begin(), type_func),
                    boost::make_transform_iterator(hand->end(), type_func)) :
                CardTypeVector(
                    std::distance(hand->begin(), hand->end()), std::nullopt);
            cards.emplace(position.value(), cards_in_hand);
        }
    }
    return cards;
}

auto getTrick(const BridgeEngine& engine)
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
    return cards;
}

auto getTricksWon(const BridgeEngine& engine)
{
    return engine.getTricksWon().value_or(TricksWon {0, 0});
}

auto getVulnerability(const DuplicateGameManager& gameManager)
{
    return gameManager.getVulnerability();
}

}

nlohmann::json getGameState(
    const Bridge::Player& player,
    const Engine::BridgeEngine& engine,
    const Engine::DuplicateGameManager& gameManager,
    std::optional<std::vector<std::string>> keys)
{
    auto state = nlohmann::json::object();
    if (keys) {
        // keys will be binary searched below
        std::sort(keys->begin(), keys->end());
    }
    auto include_key = [&keys](const auto& key)
    {
        // keys are sorted above
        return !keys || std::binary_search(keys->begin(), keys->end(), key);
    };
    if (include_key(POSITION_COMMAND)) {
        state.emplace(POSITION_COMMAND, getPosition(engine, player));
    }
    if (include_key(POSITION_IN_TURN_COMMAND)) {
        state.emplace(POSITION_IN_TURN_COMMAND, getPositionInTurn(engine));
    }
    if (include_key(DECLARER_COMMAND)) {
        state.emplace(
            DECLARER_COMMAND,
            getBiddingResult(engine, &Bidding::getDeclarerPosition));
    }
    if (include_key(CONTRACT_COMMAND)) {
        state.emplace(
            CONTRACT_COMMAND,
            getBiddingResult(engine, &Bidding::getContract));
    }
    if (include_key(ALLOWED_CALLS_COMMAND)) {
        state.emplace(
            ALLOWED_CALLS_COMMAND, getAllowedCalls(engine, player));
    }
    if (include_key(CALLS_COMMAND)) {
        state.emplace(CALLS_COMMAND, getCalls(engine));
    }
    if (include_key(ALLOWED_CARDS_COMMAND)) {
        state.emplace(
            ALLOWED_CARDS_COMMAND, getAllowedCards(engine, player));
    }
    if (include_key(CARDS_COMMAND)) {
        state.emplace(CARDS_COMMAND, getCards(player, engine));
    }
    if (include_key(TRICK_COMMAND)) {
        state.emplace(TRICK_COMMAND, getTrick(engine));
    }
    if (include_key(TRICKS_WON_COMMAND)) {
        state.emplace(TRICKS_WON_COMMAND, getTricksWon(engine));
    }
    if (include_key(VULNERABILITY_COMMAND)) {
        state.emplace(VULNERABILITY_COMMAND, getVulnerability(gameManager));
    }
    return state;
}

}
}
