#include "bridge/DealState.hh"

#include "IoUtility.hh"

#include <algorithm>
#include <iterator>
#include <ostream>

namespace Bridge {

bool operator==(
    const DealState::BiddingResult& lhs, const DealState::BiddingResult& rhs)
{
    return &lhs == &rhs || (
        lhs.declarer == rhs.declarer && lhs.contract == rhs.contract);
}

bool operator==(
    const DealState::PlayingState& lhs, const DealState::PlayingState& rhs)
{
    return &lhs == &rhs || (
        lhs.currentTrick == rhs.currentTrick &&
        lhs.dealResult == rhs.dealResult);
}

bool operator==(const DealState& lhs, const DealState& rhs)
{
    return &lhs == &rhs || (
        lhs.stage == rhs.stage &&
        lhs.vulnerability == rhs.vulnerability &&
        lhs.positionInTurn == rhs.positionInTurn &&
        lhs.cards == rhs.cards &&
        lhs.calls == rhs.calls &&
        lhs.biddingResult == rhs.biddingResult &&
        lhs.playingResult == rhs.playingResult);
}

std::ostream& operator<<(std::ostream& os, const Stage stage)
{
    static const auto STAGE_NAMES = std::map<Stage, const char*>{
        { Stage::SHUFFLING, "shuffling" },
        { Stage::BIDDING,   "bidding"   },
        { Stage::PLAYING,   "playing"   },
        { Stage::ENDED,     "ended"     },
    };
    return outputEnum(os, stage, STAGE_NAMES);
}

std::ostream& operator<<(std::ostream& os, const DealState& state)
{
    os << "Deal state";
    os << "\nStage: " << state.stage;
    if (const auto& position_in_turn = state.positionInTurn) {
        os << "\nIn turn: " << *position_in_turn;
    }
    if (const auto& vulnerability = state.vulnerability) {
        os << "\nVulnerability: " << *vulnerability;
    }
    if (const auto& cards = state.cards) {
        os << "\nCards:";
        for (const auto card : *cards) {
            os << "\n  " << card.first << ": ";
            std::copy(card.second.begin(), card.second.end(),
                      std::ostream_iterator<CardType>(os, ", "));
        }
    }
    if (const auto& calls = state.calls) {
        os << "\nCalls:";
        for (const auto call : *calls) {
            os << "\n  " << call.first << ": " << call.second;
        }
    }
    if (const auto& bidding_result = state.biddingResult) {
        os << "\nBidding result:";
        os << "\n  Declarer: " << bidding_result->declarer;
        os << "\n  Contract: " << bidding_result->contract;
    }
    if (const auto& playing_result = state.playingResult) {
        os << "\nPlaying result:";
        os << "\n  Current trick:";
        for (const auto card_in_trick : playing_result->currentTrick) {
            os << "\n    " << card_in_trick.first << ": "
               << card_in_trick.second;
        }
        os << "\n  Tricks won by north-south: "
           << playing_result->dealResult.tricksWonByNorthSouth;
        os << "\n  Tricks won by east-west: "
           << playing_result->dealResult.tricksWonByEastWest;
    }
    return os;
}

}
