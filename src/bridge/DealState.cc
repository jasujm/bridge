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

bool operator==(const DealState& lhs, const DealState& rhs)
{
    return &lhs == &rhs || (
        lhs.stage == rhs.stage &&
        lhs.vulnerability == rhs.vulnerability &&
        lhs.positionInTurn == rhs.positionInTurn &&
        lhs.cards == rhs.cards &&
        lhs.calls == rhs.calls &&
        lhs.biddingResult == rhs.biddingResult &&
        lhs.currentTrick == rhs.currentTrick &&
        lhs.tricksWon == rhs.tricksWon);
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
        os << "\nNorth-south vulnerable: " <<
            vulnerability->northSouthVulnerable;
        os << "\nEast-west vulnerable: " <<
            vulnerability->eastWestVulnerable;
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
    if (const auto& current_trick = state.currentTrick){
        os << "\nCurrent trick:";
        for (const auto& card_in_trick : *current_trick) {
            os << "\n  " << card_in_trick.first << ": "
               << card_in_trick.second;
        }
    }
    if (const auto& tricks_won = state.tricksWon) {
        os << "\nTricks won by north-south: "
           << tricks_won->tricksWonByNorthSouth;
        os << "\nTricks won by east-west: "
           << tricks_won->tricksWonByEastWest;
    }
    return os;
}

}
