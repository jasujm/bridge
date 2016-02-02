#include "engine/MakeDealState.hh"

#include "bridge/Bidding.hh"
#include "bridge/Card.hh"
#include "bridge/CardType.hh"
#include "bridge/DealState.hh"
#include "bridge/Hand.hh"
#include "bridge/Trick.hh"
#include "engine/BridgeEngine.hh"
#include "Utility.hh"

#include <boost/logic/tribool.hpp>

#include <utility>

namespace Bridge {
namespace Engine {

namespace {

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

DealState makeDealState(const BridgeEngine& engine)
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

    // Fill cards
    for (const auto position : POSITIONS) {
        const auto& player = engine.getPlayer(position);
        if (const auto hand = engine.getHand(player)) {
            if (!state.cards) {
                state.cards.emplace();
            }
            fillCards(state, position, *hand);
        }
    }

    // If there were no cards, we assume that the stage is shuffling
    if (!state.cards) {
        state.stage = Stage::SHUFFLING;
        return state;
    }

    // Fill bidding
    if (const auto bidding = engine.getBidding()) {
        state.stage = Stage::BIDDING;
        fillBidding(state, *bidding);
        if (bidding->hasContract()) {
            fillContract(state, *bidding);
        }
    }

    // Fill current trick
    if (const auto current_trick = engine.getCurrentTrick()) {
        state.stage = Stage::PLAYING;
        fillTricks(state, *current_trick, engine);
    }

    // Fill tricks won by each partnership
    if (const auto tricks_won = engine.getTricksWon()) {
        state.tricksWon.emplace(*tricks_won);
    }

    return state;
}

}
}
