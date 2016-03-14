#include "engine/MakeDealState.hh"

#include "bridge/BidIterator.hh"
#include "bridge/Bidding.hh"
#include "bridge/Card.hh"
#include "bridge/CardType.hh"
#include "bridge/DealState.hh"
#include "bridge/Hand.hh"
#include "bridge/Trick.hh"
#include "engine/BridgeEngine.hh"
#include "Utility.hh"

#include <boost/function_output_iterator.hpp>
#include <boost/logic/tribool.hpp>

#include <algorithm>
#include <utility>

namespace Bridge {
namespace Engine {

namespace {

void fillAllowedCalls(DealState& state, const Bidding& bidding)
{
    auto calls = DealState::AllowedCalls {Pass {}};
    if (bidding.isDoublingAllowed()) {
        calls.emplace_back(Double {});
    }
    if (bidding.isRedoublingAllowed()) {
        calls.emplace_back(Redouble {});
    }
    std::copy(
        BidIterator(bidding.getLowestAllowedBid()),
        BidIterator(boost::none),
        std::back_inserter(calls));
    state.allowedCalls.emplace(std::move(calls));
}

void fillAllowedCards(
    DealState& state, const Hand& hand, const Trick& trick)
{
    auto cards = DealState::AllowedCards {};
    for (const auto& card : hand) {
        if (trick.canPlay(hand, card)) {
            // If card can be played, it's type is known
            cards.emplace_back(card.getType().get());
        }
    }
    state.allowedCards.emplace(std::move(cards));
}

void fillCards(DealState& state, const Position position, const Hand& hand)
{
    auto cards = std::vector<CardType> {};
    for (const auto& card : hand) {
        if (const auto type = card.getType()) {
            cards.emplace_back(*type);
        }
    }
    state.cards->emplace(position, std::move(cards));
}

void fillBidding(DealState& state, const Bidding& bidding)
{
    state.calls.emplace(bidding.begin(), bidding.end());
}

void fillContract(DealState& state, const Bidding& bidding) {
    // We assume here that if bidding is available and has ended, the deal has
    // not been passed out, so we can dereference twice.
    state.declarer.emplace(
        dereference(dereference(bidding.getDeclarerPosition())));
    state.contract.emplace(dereference(dereference(bidding.getContract())));
}

void fillTricks(
    DealState& state, const Trick& currentTrick, const BridgeEngine& engine)
{
    state.currentTrick.emplace();
    for (const auto pair : currentTrick) {
        if (const auto type = pair.second.getType()) {
            // We assume it is safe to dereference as we are in the deal phase
            const auto position = dereference(engine.getPosition(pair.first));
            state.currentTrick->emplace_back(position, *type);
        }
    }
}

}

DealState makeDealState(const BridgeEngine& engine, const Player& player)
{
    DealState state;

    if (engine.hasEnded()) {
        state.stage = Stage::ENDED;
        return state;
    }

    state.vulnerability = engine.getVulnerability();

    if (const auto player = engine.getPlayerInTurn()) {
        state.positionInTurn = engine.getPosition(*player);
    }

    // If no one has turn, assume shuffling
    if (!state.positionInTurn) {
        state.stage = Stage::SHUFFLING;
        return state;
    }

    // Fill cards
    {
        state.cards.emplace();
        auto fill_cards = [&state, &engine](const auto& hand)
        {
            const auto position = dereference(engine.getPosition(hand));
            fillCards(state, position, hand);
        };
        engine.getVisibleHands(
            player, boost::make_function_output_iterator(fill_cards));
    }

    // Fill bidding
    const auto position = engine.getPosition(player);
    if (const auto bidding = engine.getBidding()) {
        state.stage = Stage::BIDDING;
        fillBidding(state, *bidding);
        if (bidding->hasContract()) {
            fillContract(state, *bidding);
        } else if (position == state.positionInTurn) {
            fillAllowedCalls(state, *bidding);
        }
    }

    // Fill current trick
    if (const auto current_trick = engine.getCurrentTrick()) {
        state.stage = Stage::PLAYING;
        fillTricks(state, *current_trick, engine);
        if (position == state.positionInTurn) {
            if (const auto player = engine.getPlayerInTurn()) {
                if (const auto hand = engine.getHand(*player)) {
                    fillAllowedCards(state, *hand, *current_trick);
                }
            }
        }
    }

    // Fill tricks won by each partnership
    if (const auto tricks_won = engine.getTricksWon()) {
        state.tricksWon.emplace(*tricks_won);
    }

    return state;
}

}
}
