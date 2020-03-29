#include "main/GameStateHelper.hh"

#include "bridge/AllowedCalls.hh"
#include "bridge/AllowedCards.hh"
#include "bridge/Call.hh"
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

using CardTypeVector = std::vector<std::optional<CardType>>;

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

auto getPublicCards(const BridgeEngine& engine)
{
    auto cards = nlohmann::json::object();
    for (const auto position : Position::all()) {
        if (const auto hand = engine.getHand(position)) {
            const auto cards_in_hand = engine.isVisibleToAll(*hand) ?
                getCardsFromHand(*hand) :
                CardTypeVector(
                    std::distance(hand->begin(), hand->end()), std::nullopt);
            cards.emplace(position.value(), cards_in_hand);
        }
    }
    return cards;
}

auto getPrivateCards(const BridgeEngine& engine, const Player& player)
{
    if (const auto position = engine.getPosition(player)) {
        if (const auto hand = engine.getHand(*position)) {
            return nlohmann::json {
                { *position, getCardsFromHand(*hand) },
            };
        }
    }
    return nlohmann::json::object();
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

auto getPubstateSubobject(
    const Engine::BridgeEngine& engine,
    const Engine::DuplicateGameManager& gameManager)
{
    return nlohmann::json {
        { POSITION_IN_TURN_COMMAND, getPositionInTurn(engine) },
        { DECLARER_COMMAND, getBiddingResult(engine, &Bidding::getDeclarerPosition) },
        { CONTRACT_COMMAND, getBiddingResult(engine, &Bidding::getContract) },
        { CALLS_COMMAND, getCalls(engine) },
        { CARDS_COMMAND, getPublicCards(engine) },
        { TRICK_COMMAND, getTrick(engine) },
        { TRICKS_WON_COMMAND, getTricksWon(engine) },
        { VULNERABILITY_COMMAND, getVulnerability(gameManager) },
    };
}

auto getPrivstateSubobject(
    const Engine::BridgeEngine& engine, const Player& player)
{
    return nlohmann::json {
        { CARDS_COMMAND, getPrivateCards(engine, player) },
    };
}

auto getSelfSubobject(const Engine::BridgeEngine& engine, const Player& player)
{
    return nlohmann::json {
        { POSITION_COMMAND, getPosition(engine, player) },
        { ALLOWED_CALLS_COMMAND, getAllowedCalls(engine, player) },
        { ALLOWED_CARDS_COMMAND, getAllowedCards(engine, player) },
    };
}

}

CardTypeVector getCardsFromHand(const Hand& hand)
{
    const auto func = [](const auto& card) { return card.getType(); };
    return std::vector(
        boost::make_transform_iterator(hand.begin(), func),
        boost::make_transform_iterator(hand.end(), func));
}

nlohmann::json getGameState(
    const Bridge::Player& player,
    const Engine::BridgeEngine& engine,
    const Engine::DuplicateGameManager& gameManager,
    std::optional<std::vector<std::string>> keys)
{
    if (keys) {
        auto state = nlohmann::json::object();
        for (const auto& key : *keys) {
            if (key == PUBSTATE_COMMAND) {
                state.emplace(key, getPubstateSubobject(engine, gameManager));
            } else if (key == PRIVSTATE_COMMAND) {
                state.emplace(key, getPrivstateSubobject(engine, player));
            } else if (key == SELF_COMMAND) {
                state.emplace(key, getSelfSubobject(engine, player));
            }
        }
        return state;
    } else {
        return {
            { PUBSTATE_COMMAND, getPubstateSubobject(engine, gameManager) },
            { PRIVSTATE_COMMAND, getPrivstateSubobject(engine, player) },
            { SELF_COMMAND, getSelfSubobject(engine, player) },
        };
    }
}

}
}
