#include "engine/MakeDealState.hh"

#include "bridge/Bidding.hh"
#include "bridge/Card.hh"
#include "bridge/CardType.hh"
#include "bridge/Deal.hh"
#include "bridge/DealState.hh"
#include "bridge/Hand.hh"
#include "bridge/Partnership.hh"
#include "bridge/Trick.hh"
#include "engine/BridgeEngine.hh"
#include "Utility.hh"

#include <boost/logic/tribool.hpp>

#include <algorithm>
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

void fillTricks(DealState& state, const Trick& currentTrick, const Deal& deal)
{
    state.currentTrick.emplace();
    for (const auto pair : currentTrick) {
        if (const auto type = pair.second.getType()) {
            // We assume it is safe to dereference as we are in the deal phase
            const auto position = dereference(deal.getPosition(pair.first));
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

    const auto* deal = engine.getCurrentDeal();
    if (!deal) {
        state.stage = Stage::SHUFFLING;
        return state;
    }

    state.vulnerability = deal->getVulnerability();
    state.positionInTurn = deal->getPositionInTurn();

    // Fill cards
    {
        const auto player_position = engine.getPosition(player);
        state.cards.emplace();
        for (const auto position : Position::all()) {
            const auto& hand = deal->getHand(position);
            if (player_position == position || deal->isVisibleToAll(position)) {
                fillCards(state, position, hand);
            }
        }
    }

    // Fill bidding
    state.stage = Stage::BIDDING;
    const auto& bidding = deal->getBidding();
    fillBidding(state, bidding);
    if (bidding.hasContract()) {
        fillContract(state, bidding);
    }

    // Fill current trick
    if (const auto current_trick = deal->getCurrentTrick()) {
        state.stage = Stage::PLAYING;
        fillTricks(state, *current_trick, *deal);

        // Fill tricks won by each partnership
        state.tricksWon.emplace(0, 0);
        for (const auto n : to(deal->getNumberOfTricks())) {
            const auto winner_position = deal->getWinnerOfTrick(n);
            if (winner_position) {
                switch (partnershipFor(*winner_position).get()) {
                case PartnershipLabel::NORTH_SOUTH:
                    ++state.tricksWon->tricksWonByNorthSouth;
                    break;
                case PartnershipLabel::EAST_WEST:
                    ++state.tricksWon->tricksWonByEastWest;
                }
            }
        }
    }

    return state;
}

}
}
