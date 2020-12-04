#include "bridge/DealState.hh"

#include "IoUtility.hh"

#include <algorithm>
#include <iterator>
#include <ostream>

namespace Bridge {

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
        lhs.currentTrick == rhs.currentTrick);
}

std::ostream& operator<<(std::ostream& os, const Stage stage)
{
    switch (stage) {
    case Stage::SHUFFLING: os << "shuffling"; break;
    case Stage::BIDDING: os << "bidding"; break;
    case Stage::PLAYING: os << "playing"; break;
    case Stage::ENDED: os << "ended"; break;
    }
    return os;
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
    return os;
}

}
