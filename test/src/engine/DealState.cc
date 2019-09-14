#include "bridge/DealState.hh"

#include "IoUtility.hh"

#include <algorithm>
#include <iterator>
#include <ostream>

namespace Bridge {

namespace {

using namespace std::string_literals;
using StageStringRelation = StageToStringMap::value_type;

const auto STAGE_STRING_PAIRS = {
    StageStringRelation { Stage::SHUFFLING, "shuffling"s },
    StageStringRelation { Stage::BIDDING,   "bidding"s   },
    StageStringRelation { Stage::PLAYING,   "playing"s   },
    StageStringRelation { Stage::ENDED,     "ended"s     },
};

}

const StageToStringMap STAGE_TO_STRING_MAP(
    STAGE_STRING_PAIRS.begin(), STAGE_STRING_PAIRS.end());

bool operator==(const DealState& lhs, const DealState& rhs)
{
    return &lhs == &rhs || (
        lhs.stage == rhs.stage &&
        lhs.vulnerability == rhs.vulnerability &&
        lhs.positionInTurn == rhs.positionInTurn &&
        lhs.cards == rhs.cards &&
        lhs.calls == rhs.calls &&
        lhs.declarer == rhs.declarer &&
        lhs.contract == rhs.contract &&
        lhs.currentTrick == rhs.currentTrick &&
        lhs.tricksWon == rhs.tricksWon);
}

std::ostream& operator<<(std::ostream& os, const Stage stage)
{
    return outputEnum(os, stage, STAGE_TO_STRING_MAP.left);
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
    if (const auto& declarer = state.declarer) {
        os << "\nDeclarer: " << *declarer;
    }
    if (const auto& contract = state.contract) {
        os << "\nContract: " << *contract;
    }
    if (const auto& current_trick = state.currentTrick) {
        os << "\nCurrent trick:";
        for (const auto& card_in_trick : *current_trick) {
            os << "\n  " << card_in_trick.first << ": "
               << card_in_trick.second;
        }
    }
    if (const auto& tricks_won = state.tricksWon) {
        os << "\nTricks won: " << *tricks_won;
    }
    return os;
}

}
