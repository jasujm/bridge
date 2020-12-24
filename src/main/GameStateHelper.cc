#include "main/GameStateHelper.hh"

#include "bridge/AllowedCalls.hh"
#include "bridge/AllowedCards.hh"
#include "bridge/Call.hh"
#include "bridge/CardsForPosition.hh"
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

#include <boost/range/adaptor/transformed.hpp>
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

auto getPublicCardsForPosition(const Deal& deal, const Position position)
{
    const auto& hand = deal.getHand(position);
    return deal.isVisibleToAll(position) ? getCardsFromHand(hand) :
        CardTypeVector(std::distance(hand.begin(), hand.end()), std::nullopt);
}

auto getAllCardsForPosition(const Deal& deal, const Position position)
{
    const auto card_indices = cardsFor(position);
    const auto cards_for_position = card_indices |
        boost::adaptors::transformed(
            [&deal](const auto n) { return deal.getCard(n).getType(); });
    return std::vector(cards_for_position.begin(), cards_for_position.end());
}

auto getPublicCards(const Deal& deal)
{
    const auto has_ended = (deal.getPhase() == DealPhases::ENDED);
    auto cards = nlohmann::json::object();
    for (const auto position : Position::all()) {
        auto cards_in_hand = has_ended ? getAllCardsForPosition(deal, position) :
            getPublicCardsForPosition(deal, position);
        cards.emplace(position.value(), std::move(cards_in_hand));
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
    const auto has_ended = (deal.getPhase() == DealPhases::ENDED);
    for (const auto n : to(n_tricks)) {
        auto trick_object = nlohmann::json::object();
        // Last and second-to-last trick are visible
        if (has_ended || n_tricks - n <= 2) {
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

}

CardTypeVector getCardsFromHand(const Hand& hand)
{
    const auto func = [](const auto& card) { return card.getType(); };
    return std::vector(
        boost::make_transform_iterator(hand.begin(), func),
        boost::make_transform_iterator(hand.end(), func));
}

void emplacePubstate(const Deal* const deal, nlohmann::json& out)
{
    if (!deal) {
        out.emplace(PUBSTATE_COMMAND, nlohmann::json {});
    } else {
        out.emplace(
            PUBSTATE_COMMAND,
            nlohmann::json {
                { DEAL_COMMAND, deal->getUuid() },
                { PHASE_COMMAND, deal->getPhase() },
                { POSITION_IN_TURN_COMMAND, deal->getPositionInTurn() },
                { DECLARER_COMMAND, getBiddingResult(*deal, &Bidding::getDeclarerPosition) },
                { CONTRACT_COMMAND, getBiddingResult(*deal, &Bidding::getContract) },
                { CALLS_COMMAND, getCalls(*deal) },
                { CARDS_COMMAND, getPublicCards(*deal) },
                { TRICKS_COMMAND, getTricks(*deal) },
                { VULNERABILITY_COMMAND, deal->getVulnerability() },
            });
    }
}

void emplacePrivstate(
    const Deal* const deal, const std::optional<Position>& playerPosition,
    nlohmann::json& out)
{
    if (!deal) {
        out.emplace(PRIVSTATE_COMMAND, nlohmann::json {});
    } else if (!playerPosition) {
        out.emplace(PRIVSTATE_COMMAND, nlohmann::json::object());
    } else {
        out.emplace(
            PRIVSTATE_COMMAND,
            nlohmann::json {
                { CARDS_COMMAND, getPrivateCards(*deal, *playerPosition) },
            });
    }
}

void emplaceSelf(
    const Deal* const deal, const std::optional<Position>& playerPosition,
    nlohmann::json& out)
{
    const auto playerHasTurn = deal && playerPosition &&
        playerPosition == deal->getPositionInTurn();
    using j = nlohmann::json;
    out.emplace(
        SELF_COMMAND,
        j {
            { POSITION_COMMAND, playerPosition },
            {
                ALLOWED_CALLS_COMMAND,
                playerHasTurn ? j(getAllowedCalls(*deal)) : j::array(),
            },
            {
                ALLOWED_CARDS_COMMAND,
                playerHasTurn ? j(getAllowedCards(*deal)) : j::array(),
            },
        });
}

}
}
