#include "scoring/DuplicateScoring.hh"

#include "bridge/Contract.hh"
#include "bridge/Partnership.hh"
#include "scoring/DuplicateScore.hh"

#include <boost/algorithm/clamp.hpp>

#include <algorithm>
#include <map>

namespace Bridge {
namespace Scoring {

namespace {

static const auto SCORE_PER_TRICK = std::map<Strain, int>{
    { Strain::CLUBS,    20 },
    { Strain::DIAMONDS, 20 },
    { Strain::HEARTS,   30 },
    { Strain::SPADES,   30 },
    { Strain::NO_TRUMP, 30 },
};

int contractPoints(
    const Strain strain, const Doubling doubling, const int oddContractTricks)
{
    static const auto FACTOR = std::map<Doubling, int>{
        { Doubling::UNDOUBLED, 1 },
        { Doubling::DOUBLED,   2 },
        { Doubling::REDOUBLED, 4 },
    };
    const auto no_trump_bonus = (strain == Strain::NO_TRUMP) ? 10 : 0;
    return FACTOR.at(doubling) *
        (no_trump_bonus + oddContractTricks * SCORE_PER_TRICK.at(strain));
}

int overtrickPoints(
    const Strain strain, const Doubling doubling, const bool vulnerable,
    const int overtricks)
{
    static const auto FACTOR = std::map<std::pair<Doubling, bool>, int>{
        { { Doubling::DOUBLED,   false }, 100 },
        { { Doubling::REDOUBLED, false }, 200 },
        { { Doubling::DOUBLED,   true },  200 },
        { { Doubling::REDOUBLED, true },  400 },
    };
    const auto factor_iter = FACTOR.find(std::make_pair(doubling, vulnerable));
    const auto factor =
        (factor_iter == FACTOR.end()) ?
        SCORE_PER_TRICK.at(strain) : factor_iter->second;
    return overtricks * factor;
}

int gameBonus(const int contractPoints, const bool vulnerable)
{
    if (contractPoints >= 100) {
        return vulnerable ? 450 : 250;
    }
    return 0;
}

int slamBonus(const int tricksBidAndMade, const bool vulnerable)
{
    if (tricksBidAndMade == 12) {
        return vulnerable ? 750 : 500;
    } else if (tricksBidAndMade == 13) {
        return vulnerable ? 1500 : 1000;
    }
    return 0;
}

int calculateDuplicateScoreForMadeContract(
    const Contract& contract, const bool vulnerable, const int tricksWon)
{
    const auto tricks_bid_and_made =
        std::min(tricksWon, contract.bid.level + N_TRICKS_IN_BOOK);
    const auto odd_contract_tricks = tricks_bid_and_made - N_TRICKS_IN_BOOK;
    const auto overtricks = tricksWon - tricks_bid_and_made;
    const auto strain = contract.bid.strain;
    const auto doubling = contract.doubling;
    const auto contract_points =
        contractPoints(strain, doubling, odd_contract_tricks);
    const auto overtrick_points =
        overtrickPoints(strain, doubling, vulnerable, overtricks);
    constexpr auto partial_game_bonus = 50;
    const auto game_bonus = gameBonus(contract_points, vulnerable);
    const auto slam_bonus = slamBonus(tricks_bid_and_made, vulnerable);
    const auto doubling_bonus = (doubling == Doubling::DOUBLED) ? 50 : 0;
    const auto redoubling_bonus = (doubling == Doubling::REDOUBLED) ? 100 : 0;
    return contract_points +
        overtrick_points +
        partial_game_bonus +
        game_bonus +
        slam_bonus +
        doubling_bonus +
        redoubling_bonus;
}

int penaltyForFirstUndertrick(const Doubling doubling, const bool vulnerable)
{
    static const auto FACTOR = std::map<std::pair<Doubling, bool>, int>{
        { { Doubling::UNDOUBLED, false }, 50  },
        { { Doubling::DOUBLED,   false }, 100 },
        { { Doubling::REDOUBLED, false }, 200 },
        { { Doubling::UNDOUBLED, true  }, 100 },
        { { Doubling::DOUBLED,   true  }, 200 },
        { { Doubling::REDOUBLED, true  }, 400 },
    };
    return FACTOR.at(std::make_pair(doubling, vulnerable));
}

int penaltyForSecondAndThirdUndertricks(
    const Doubling doubling, const bool vulnerable, const int undertricks)
{
    static const auto FACTOR = std::map<std::pair<Doubling, bool>, int>{
        { { Doubling::UNDOUBLED, false }, 50  },
        { { Doubling::DOUBLED,   false }, 200 },
        { { Doubling::REDOUBLED, false }, 400 },
        { { Doubling::UNDOUBLED, true  }, 100 },
        { { Doubling::DOUBLED,   true  }, 300 },
        { { Doubling::REDOUBLED, true  }, 600 },
    };
    return FACTOR.at(std::make_pair(doubling, vulnerable)) * undertricks;
}

int penaltyForSubsequentUndertricks(
    const Doubling doubling, const int undertricks)
{
    static const auto FACTOR = std::map<Doubling, int>{
        { Doubling::UNDOUBLED, 50  },
        { Doubling::DOUBLED,   300 },
        { Doubling::REDOUBLED, 600 },
    };
    return FACTOR.at(doubling) * undertricks;
}

int calculateDuplicateScoreForDefeatedContract(
    const Contract& contract, const bool vulnerable, const int tricksWon)
{
    const auto undertricks = N_TRICKS_IN_BOOK + contract.bid.level - tricksWon;
    const auto second_and_third_undertricks =
        boost::algorithm::clamp(undertricks - 1, 0, 2);
    const auto subsequent_undertricks = std::max(undertricks - 3, 0);
    const auto doubling = contract.doubling;
    return penaltyForFirstUndertrick(doubling, vulnerable) +
        penaltyForSecondAndThirdUndertricks(
            doubling, vulnerable, second_and_third_undertricks) +
        penaltyForSubsequentUndertricks(
            doubling, subsequent_undertricks);
}

}

DuplicateScore calculateDuplicateScore(
    const Partnership partnership, const Contract& contract,
    const bool vulnerable, const int tricksWon)
{
    if (isMade(contract, tricksWon)) {
        return {
            partnership,
            calculateDuplicateScoreForMadeContract(
                contract, vulnerable, tricksWon)};
    } else {
        return {
            otherPartnership(partnership),
            calculateDuplicateScoreForDefeatedContract(
                contract, vulnerable, tricksWon)};
    }
}

}
}
