#include "bridge/Bid.hh"
#include "bridge/Contract.hh"
#include "bridge/Partnership.hh"
#include "scoring/DuplicateScore.hh"
#include "scoring/DuplicateScoring.hh"

#include <gtest/gtest.h>

#include <utility>
#include <string>

using Bridge::Doubling;

namespace Partnerships = Bridge::Partnerships;
namespace Strains = Bridge::Strains;

namespace {

void test(
    const bool won, const int score, const int level,
    const Bridge::Strain strain, const Doubling doubling, const bool vulnerable,
    const int tricksWon, const std::string& message)
{
    const auto winner_partnership = won ?
        Partnerships::NORTH_SOUTH : Partnerships::EAST_WEST;
    EXPECT_EQ(
        Bridge::Scoring::DuplicateScore(winner_partnership, score),
        Bridge::Scoring::calculateDuplicateScore(
            Partnerships::NORTH_SOUTH,
            Bridge::Contract {Bridge::Bid {level, strain}, doubling},
            vulnerable, tricksWon)) << message;
}

}

TEST(DuplicateScoringTest, testUndoubledMadeContracts)
{
    test(true, 70, 1, Strains::CLUBS, Doubling::UNDOUBLED, false, 7, "clubs part-score");
    test(true, 90, 1, Strains::DIAMONDS, Doubling::UNDOUBLED, false, 8, "diamonds part-score");
    test(true, 140, 1, Strains::HEARTS, Doubling::UNDOUBLED, false, 9, "hearts part-score");
    test(true, 170, 1, Strains::SPADES, Doubling::UNDOUBLED, false, 10, "spades part-score");
    test(true, 210, 1, Strains::NO_TRUMP, Doubling::UNDOUBLED, false, 11, "notrump part-score");
    test(true, 240, 1, Strains::NO_TRUMP, Doubling::UNDOUBLED, false, 12, "notrump part-score 2");
    test(true, 400, 3, Strains::NO_TRUMP, Doubling::UNDOUBLED, false, 9, "notrump game");
    test(true, 420, 4, Strains::HEARTS, Doubling::UNDOUBLED, false, 10, "major suit game");
    test(true, 400, 5, Strains::CLUBS, Doubling::UNDOUBLED, false, 11, "minor suit game");
    test(true, 920, 6, Strains::CLUBS, Doubling::UNDOUBLED, false, 12, "small slam");
    test(true, 1520, 7, Strains::NO_TRUMP, Doubling::UNDOUBLED, false, 13, "grand slam");
}

TEST(DuplicateScoringTest, testDoubledMadeContracts)
{
    test(true, 180, 1, Strains::NO_TRUMP, Doubling::DOUBLED, false, 7, "no overtricks");
    test(true, 280, 1, Strains::NO_TRUMP, Doubling::DOUBLED, false, 8, "one overtrick");
    test(true, 490, 2, Strains::NO_TRUMP, Doubling::DOUBLED, false, 8, "game");
}

TEST(DuplicateScoringTest, testRedoubledMadeContracts)
{
    test(true, 230, 1, Strains::CLUBS, Doubling::REDOUBLED, false, 7, "no overtricks");
    test(true, 430, 1, Strains::CLUBS, Doubling::REDOUBLED, false, 8, "one overtrick");
    test(true, 560, 2, Strains::CLUBS, Doubling::REDOUBLED, false, 8, "game");
}

TEST(DuplicateScoringTest, testVulnerableMadeContracts)
{
    test(true, 90, 1, Strains::CLUBS, Doubling::UNDOUBLED, true, 8, "undoubled");
    test(true, 340, 1, Strains::CLUBS, Doubling::DOUBLED, true, 8, "doubled");
    test(true, 630, 1, Strains::CLUBS, Doubling::REDOUBLED, true, 8, "redoubled");
    test(true, 600, 3, Strains::NO_TRUMP, Doubling::UNDOUBLED, true, 9, "game");
    test(true, 1440, 6, Strains::NO_TRUMP, Doubling::UNDOUBLED, true, 12, "small slam");
    test(true, 2220, 7, Strains::NO_TRUMP, Doubling::UNDOUBLED, true, 13, "grand slam");
}

TEST(DuplicateScoringTest, testUndoubledDefeatedContracts)
{
    test(false, 50, 1, Strains::CLUBS, Doubling::UNDOUBLED, false, 6, "one undertrick");
    test(false, 100, 2, Strains::CLUBS, Doubling::UNDOUBLED, false, 6, "two undertricks");
    test(false, 150, 2, Strains::CLUBS, Doubling::UNDOUBLED, false, 5, "three undertricks");
}

TEST(DuplicateScoringTest, testDoubledDefeatedContracts)
{
    test(false, 100, 1, Strains::CLUBS, Doubling::DOUBLED, false, 6, "one undertrick");
    test(false, 300, 2, Strains::CLUBS, Doubling::DOUBLED, false, 6, "two undertricks");
    test(false, 500, 3, Strains::CLUBS, Doubling::DOUBLED, false, 6, "three undertricks");
    test(false, 800, 4, Strains::CLUBS, Doubling::DOUBLED, false, 6, "four undertricks");
    test(false, 1100, 5, Strains::CLUBS, Doubling::DOUBLED, false, 6, "five undertricks");
}

TEST(DuplicateScoringTest, testRedoubledDefeatedContracts)
{
    test(false, 200, 1, Strains::CLUBS, Doubling::REDOUBLED, false, 6, "one undertrick");
    test(false, 600, 2, Strains::CLUBS, Doubling::REDOUBLED, false, 6, "two undertricks");
    test(false, 1000, 3, Strains::CLUBS, Doubling::REDOUBLED, false, 6, "three undertricks");
    test(false, 1600, 4, Strains::CLUBS, Doubling::REDOUBLED, false, 6, "four undertricks");
    test(false, 2200, 5, Strains::CLUBS, Doubling::REDOUBLED, false, 6, "five undertricks");
}

TEST(DuplicateScoringTest, testVulnerableUndoubledDefeatedContracts)
{
    test(false, 100, 1, Strains::CLUBS, Doubling::UNDOUBLED, true, 6, "one undertrick");
    test(false, 200, 2, Strains::CLUBS, Doubling::UNDOUBLED, true, 6, "two undertricks");
    test(false, 300, 2, Strains::CLUBS, Doubling::UNDOUBLED, true, 5, "three undertricks");
}

TEST(DuplicateScoringTest, testVulnerableDoubledDefeatedContracts)
{
    test(false, 200, 1, Strains::CLUBS, Doubling::DOUBLED, true, 6, "one undertrick");
    test(false, 500, 2, Strains::CLUBS, Doubling::DOUBLED, true, 6, "two undertricks");
    test(false, 800, 3, Strains::CLUBS, Doubling::DOUBLED, true, 6, "three undertricks");
    test(false, 1100, 4, Strains::CLUBS, Doubling::DOUBLED, true, 6, "four undertricks");
    test(false, 1400, 5, Strains::CLUBS, Doubling::DOUBLED, true, 6, "five undertricks");
}

TEST(DuplicateScoringTest, testVulnerableDefeatedContracts)
{
    test(false, 400, 1, Strains::CLUBS, Doubling::REDOUBLED, true, 6, "one undertrick");
    test(false, 1000, 2, Strains::CLUBS, Doubling::REDOUBLED, true, 6, "two undertricks");
    test(false, 1600, 3, Strains::CLUBS, Doubling::REDOUBLED, true, 6, "three undertricks");
    test(false, 2200, 4, Strains::CLUBS, Doubling::REDOUBLED, true, 6, "four undertricks");
    test(false, 2800, 5, Strains::CLUBS, Doubling::REDOUBLED, true, 6, "five undertricks");
}
