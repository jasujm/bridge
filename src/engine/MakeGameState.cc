#include "engine/MakeGameState.hh"

#include "bridge/Bidding.hh"
#include "bridge/Card.hh"
#include "bridge/CardType.hh"
#include "bridge/GameState.hh"
#include "bridge/Hand.hh"
#include "bridge/Trick.hh"
#include "engine/BridgeEngine.hh"
#include "Utility.hh"

#include <boost/logic/tribool.hpp>

#include <utility>

namespace Bridge {
namespace Engine {

namespace {

void fillCards(GameState& state, const Position position, const Hand& hand)
{
    auto cards = std::vector<CardType> {};
    for (const auto& card : cardsIn(hand)) {
        if (card) {
            if (const auto type = card->getType()) {
                cards.emplace_back(*type);
            }
        }
    }
    state.cards->emplace(position, std::move(cards));
}

void fillBidding(GameState& state, const Bidding& bidding)
{
    auto position = bidding.getOpeningPosition();
    state.calls.emplace();
    for (const auto& call : callsIn(bidding)) {
        state.calls->emplace_back(position, call);
        position = clockwise(position);
    }
}

void fillContract(GameState& state, const Bidding& bidding) {
    // We assume here that if bidding is available and has ended, the deal has
    // not been passed out, so we can dereference twice.
    const auto declarer =
        dereference(dereference(bidding.getDeclarerPosition()));
    const auto contract = dereference(dereference(bidding.getContract()));
    const auto result = GameState::BiddingResult {declarer, contract};
    state.biddingResult.emplace(result);
}

void fillPlaying(
    GameState& state, const Trick& currentTrick, const DealResult& dealResult,
    const BridgeEngine& engine)
{
    decltype(state.playingResult->currentTrick) current_trick;
    for (const auto position : POSITIONS) {
        // We assume that the hands are available so it is safe to dereference
        const auto& hand =
            dereference(engine.getHand(engine.getPlayer(position)));
        if (const auto card = currentTrick.getCard(hand)) {
            if (const auto type = card->getType()) {
                current_trick.emplace(position, *type);
            }
        }
    }
    const auto playingResult =
        GameState::PlayingResult {current_trick, dealResult};
    state.playingResult.emplace(playingResult);
}

}

GameState makeGameState(const BridgeEngine& engine)
{
    GameState state;

    if (engine.terminated()) {
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

    // Fill playing results
    const auto current_trick = engine.getCurrentTrick();
    const auto deal_result = engine.getDealResult();
    if (current_trick && deal_result) {
        state.stage = Stage::PLAYING;
        fillPlaying(state, *current_trick, *deal_result, engine);
    }

    return state;
}

}
}
