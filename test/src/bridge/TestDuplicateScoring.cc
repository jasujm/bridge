#include "bridge/Bid.hh"
#include "bridge/Contract.hh"
#include "bridge/DuplicateScoring.hh"

#include <gtest/gtest.h>

#include <utility>
#include <string>

using Bridge::DuplicateResult;

namespace Partnerships = Bridge::Partnerships;
namespace Doublings = Bridge::Doublings;
namespace Strains = Bridge::Strains;

namespace {

void test(
    const int score, const int level, const Bridge::Strain strain,
    const Bridge::Doubling doubling, const bool vulnerable, const int tricksWon,
    const std::string& message)
{
    EXPECT_EQ(
        score,
        Bridge::calculateDuplicateScore(
            Bridge::Contract {Bridge::Bid {level, strain}, doubling},
            vulnerable, tricksWon)) << message;
}

}

TEST(DuplicateScoringTest, testUndoubledMadeContracts)
{
    test(70, 1, Strains::CLUBS, Doublings::UNDOUBLED, false, 7, "clubs part-score");
    test(90, 1, Strains::DIAMONDS, Doublings::UNDOUBLED, false, 8, "diamonds part-score");
    test(140, 1, Strains::HEARTS, Doublings::UNDOUBLED, false, 9, "hearts part-score");
    test(170, 1, Strains::SPADES, Doublings::UNDOUBLED, false, 10, "spades part-score");
    test(210, 1, Strains::NO_TRUMP, Doublings::UNDOUBLED, false, 11, "notrump part-score");
    test(240, 1, Strains::NO_TRUMP, Doublings::UNDOUBLED, false, 12, "notrump part-score 2");
    test(400, 3, Strains::NO_TRUMP, Doublings::UNDOUBLED, false, 9, "notrump game");
    test(420, 4, Strains::HEARTS, Doublings::UNDOUBLED, false, 10, "major suit game");
    test(400, 5, Strains::CLUBS, Doublings::UNDOUBLED, false, 11, "minor suit game");
    test(920, 6, Strains::CLUBS, Doublings::UNDOUBLED, false, 12, "small slam");
    test(1520, 7, Strains::NO_TRUMP, Doublings::UNDOUBLED, false, 13, "grand slam");
}

TEST(DuplicateScoringTest, testDoubledMadeContracts)
{
    test(180, 1, Strains::NO_TRUMP, Doublings::DOUBLED, false, 7, "no overtricks");
    test(280, 1, Strains::NO_TRUMP, Doublings::DOUBLED, false, 8, "one overtrick");
    test(490, 2, Strains::NO_TRUMP, Doublings::DOUBLED, false, 8, "game");
}

TEST(DuplicateScoringTest, testRedoubledMadeContracts)
{
    test(230, 1, Strains::CLUBS, Doublings::REDOUBLED, false, 7, "no overtricks");
    test(430, 1, Strains::CLUBS, Doublings::REDOUBLED, false, 8, "one overtrick");
    test(560, 2, Strains::CLUBS, Doublings::REDOUBLED, false, 8, "game");
}

TEST(DuplicateScoringTest, testVulnerableMadeContracts)
{
    test(90, 1, Strains::CLUBS, Doublings::UNDOUBLED, true, 8, "undoubled");
    test(340, 1, Strains::CLUBS, Doublings::DOUBLED, true, 8, "doubled");
    test(630, 1, Strains::CLUBS, Doublings::REDOUBLED, true, 8, "redoubled");
    test(600, 3, Strains::NO_TRUMP, Doublings::UNDOUBLED, true, 9, "game");
    test(1440, 6, Strains::NO_TRUMP, Doublings::UNDOUBLED, true, 12, "small slam");
    test(2220, 7, Strains::NO_TRUMP, Doublings::UNDOUBLED, true, 13, "grand slam");
}

TEST(DuplicateScoringTest, testUndoubledDefeatedContracts)
{
    test(-50, 1, Strains::CLUBS, Doublings::UNDOUBLED, false, 6, "one undertrick");
    test(-100, 2, Strains::CLUBS, Doublings::UNDOUBLED, false, 6, "two undertricks");
    test(-150, 2, Strains::CLUBS, Doublings::UNDOUBLED, false, 5, "three undertricks");
}

TEST(DuplicateScoringTest, testDoubledDefeatedContracts)
{
    test(-100, 1, Strains::CLUBS, Doublings::DOUBLED, false, 6, "one undertrick");
    test(-300, 2, Strains::CLUBS, Doublings::DOUBLED, false, 6, "two undertricks");
    test(-500, 3, Strains::CLUBS, Doublings::DOUBLED, false, 6, "three undertricks");
    test(-800, 4, Strains::CLUBS, Doublings::DOUBLED, false, 6, "four undertricks");
    test(-1100, 5, Strains::CLUBS, Doublings::DOUBLED, false, 6, "five undertricks");
}

TEST(DuplicateScoringTest, testRedoubledDefeatedContracts)
{
    test(-200, 1, Strains::CLUBS, Doublings::REDOUBLED, false, 6, "one undertrick");
    test(-600, 2, Strains::CLUBS, Doublings::REDOUBLED, false, 6, "two undertricks");
    test(-1000, 3, Strains::CLUBS, Doublings::REDOUBLED, false, 6, "three undertricks");
    test(-1600, 4, Strains::CLUBS, Doublings::REDOUBLED, false, 6, "four undertricks");
    test(-2200, 5, Strains::CLUBS, Doublings::REDOUBLED, false, 6, "five undertricks");
}

TEST(DuplicateScoringTest, testVulnerableUndoubledDefeatedContracts)
{
    test(-100, 1, Strains::CLUBS, Doublings::UNDOUBLED, true, 6, "one undertrick");
    test(-200, 2, Strains::CLUBS, Doublings::UNDOUBLED, true, 6, "two undertricks");
    test(-300, 2, Strains::CLUBS, Doublings::UNDOUBLED, true, 5, "three undertricks");
}

TEST(DuplicateScoringTest, testVulnerableDoubledDefeatedContracts)
{
    test(-200, 1, Strains::CLUBS, Doublings::DOUBLED, true, 6, "one undertrick");
    test(-500, 2, Strains::CLUBS, Doublings::DOUBLED, true, 6, "two undertricks");
    test(-800, 3, Strains::CLUBS, Doublings::DOUBLED, true, 6, "three undertricks");
    test(-1100, 4, Strains::CLUBS, Doublings::DOUBLED, true, 6, "four undertricks");
    test(-1400, 5, Strains::CLUBS, Doublings::DOUBLED, true, 6, "five undertricks");
}

TEST(DuplicateScoringTest, testVulnerableDefeatedContracts)
{
    test(-400, 1, Strains::CLUBS, Doublings::REDOUBLED, true, 6, "one undertrick");
    test(-1000, 2, Strains::CLUBS, Doublings::REDOUBLED, true, 6, "two undertricks");
    test(-1600, 3, Strains::CLUBS, Doublings::REDOUBLED, true, 6, "three undertricks");
    test(-2200, 4, Strains::CLUBS, Doublings::REDOUBLED, true, 6, "four undertricks");
    test(-2800, 5, Strains::CLUBS, Doublings::REDOUBLED, true, 6, "five undertricks");
}

TEST(DuplicateScoringTest, testMakeDuplicateResultDeclarer)
{
    EXPECT_EQ(
        DuplicateResult(Partnerships::NORTH_SOUTH, 100),
        makeDuplicateResult(Partnerships::NORTH_SOUTH, 100));
}

TEST(DuplicateScoringTest, testMakeDuplicateResultOpponent)
{
    EXPECT_EQ(
        DuplicateResult(Partnerships::NORTH_SOUTH, 100),
        makeDuplicateResult(Partnerships::EAST_WEST, -100));
}

TEST(DuplicateScoringTest, testMakeDuplicateResultPassedOut)
{
    EXPECT_EQ(
        DuplicateResult::passedOut(),
        makeDuplicateResult(Partnerships::NORTH_SOUTH, 0));
}
