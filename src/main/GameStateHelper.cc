#include "main/GameStateHelper.hh"

#include "bridge/AllowedCalls.hh"
#include "bridge/AllowedCards.hh"
#include "bridge/Call.hh"
#include "bridge/Contract.hh"
#include "bridge/Deal.hh"
#include "bridge/Position.hh"
#include "bridge/Trick.hh"
#include "bridge/Vulnerability.hh"
#include "main/Commands.hh"
#include "messaging/CallJsonSerializer.hh"
#include "messaging/CardTypeJsonSerializer.hh"
#include "messaging/ContractJsonSerializer.hh"
#include "messaging/JsonSerializer.hh"
#include "messaging/JsonSerializerUtility.hh"
#include "messaging/Replies.hh"
#include "messaging/SerializationUtility.hh"
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

using Messaging::JsonSerializer;

auto getAllowedCalls(const Deal& deal)
{
    auto allowed_calls = std::vector<Call> {};
    getAllowedCalls(deal.getBidding(), std::back_inserter(allowed_calls));
    return allowed_calls;
}

auto getCalls(const Deal& deal)
{
    auto calls = nlohmann::json::array();
    const auto& bidding = deal.getBidding();
    for (const auto& [ position, call ] : bidding) {
        calls.push_back(
            nlohmann::json {
                { POSITION_COMMAND, position },
                { CALL_COMMAND, call }
            });
    }
    return calls;
}

template<typename Result>
auto getBiddingResult(
    const Deal& deal,
    std::optional<std::optional<Result>> (Bidding::*getResult)() const)
{
    auto result = std::optional<Result> {};
    const auto& bidding = deal.getBidding();
    if (bidding.hasContract()) {
        result = dereference((bidding.*getResult)());
    }
    return result;
}

auto getAllowedCards(const Deal& deal)
{
    auto allowed_cards = std::vector<CardType> {};
    if (const auto trick = deal.getCurrentTrick()) {
        getAllowedCards(*trick, std::back_inserter(allowed_cards));
    }
    return allowed_cards;
}

auto getPublicCards(const Deal& deal)
{
    auto cards = nlohmann::json::object();
    for (const auto position : Position::all()) {
        const auto& hand = deal.getHand(position);
        const auto cards_in_hand = deal.isVisibleToAll(position) ?
            getCardsFromHand(hand) :
            CardTypeVector(
                std::distance(hand.begin(), hand.end()), std::nullopt);
        cards.emplace(position.value(), cards_in_hand);
    }
    return cards;
}

auto getPrivateCards(const Deal& deal, const Position position)
{
    const auto& hand = deal.getHand(position);
    return nlohmann::json {
        { position, getCardsFromHand(hand) },
    };
}

auto getTricks(const Deal& deal)
{
    auto tricks = nlohmann::json::array();
    const auto n_tricks = deal.getNumberOfTricks();
    for (const auto n : to(n_tricks)) {
        auto trick_object = nlohmann::json::object();
        // Last and second-to-last trick are visible
        if (n_tricks - n <= 2) {
            auto cards = nlohmann::json::array();
            for (const auto& [hand, card] : deal.getTrick(n)) {
                cards.emplace_back(
                    nlohmann::json {
                        { POSITION_COMMAND, deal.getPosition(hand) },
                        { CARD_COMMAND, card.getType() },
                    }
                );
            }
            trick_object.emplace(CARDS_COMMAND, std::move(cards));
        }
        trick_object.emplace(WINNER_COMMAND, deal.getWinnerOfTrick(n));
        tricks.emplace_back(std::move(trick_object));
    }
    return tricks;
}

auto getPubstateSubobject(const Deal* deal)
{
    if (!deal) {
        return nlohmann::json {};
    }

    return nlohmann::json {
        { DEAL_COMMAND, deal->getUuid() },
        { POSITION_IN_TURN_COMMAND, deal->getPositionInTurn() },
        { DECLARER_COMMAND, getBiddingResult(*deal, &Bidding::getDeclarerPosition) },
        { CONTRACT_COMMAND, getBiddingResult(*deal, &Bidding::getContract) },
        { CALLS_COMMAND, getCalls(*deal) },
        { CARDS_COMMAND, getPublicCards(*deal) },
        { TRICKS_COMMAND, getTricks(*deal) },
        { VULNERABILITY_COMMAND, deal->getVulnerability() },
    };
}

auto getPrivstateSubobject(
    const Deal* deal, const std::optional<Position>& playerPosition)
{
    if (!deal || !playerPosition) {
        return nlohmann::json {};
    }

    return nlohmann::json {
        { CARDS_COMMAND, getPrivateCards(*deal, *playerPosition) },
    };
}

auto getSelfSubobject(
    const Deal* deal, const std::optional<Position> playerPosition,
    const bool playerHasTurn)
{
    using j = nlohmann::json;
    return j {
        { POSITION_COMMAND, playerPosition },
        {
            ALLOWED_CALLS_COMMAND,
            (deal && playerHasTurn) ? j(getAllowedCalls(*deal)) : j::array(),
        },
        {
            ALLOWED_CARDS_COMMAND,
            (deal && playerHasTurn) ? j(getAllowedCards(*deal)) : j::array(),
        },
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
    const Deal* deal,
    std::optional<Position> playerPosition,
    bool playerHasTurn,
    std::optional<std::vector<std::string>> keys)
{
    if (keys) {
        auto state = nlohmann::json::object();
        for (const auto& key : *keys) {
            if (key == PUBSTATE_COMMAND) {
                state.emplace(key, getPubstateSubobject(deal));
            } else if (key == PRIVSTATE_COMMAND) {
                state.emplace(key, getPrivstateSubobject(deal, playerPosition));
            } else if (key == SELF_COMMAND) {
                state.emplace(
                    key,
                    getSelfSubobject(deal, playerPosition, playerHasTurn));
            }
        }
        return state;
    } else {
        return {
            { PUBSTATE_COMMAND, getPubstateSubobject(deal) },
            { PRIVSTATE_COMMAND, getPrivstateSubobject(deal, playerPosition) },
            {
                SELF_COMMAND,
                getSelfSubobject(deal, playerPosition, playerHasTurn)
            },
        };
    }
}

}
}
