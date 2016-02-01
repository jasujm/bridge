#include "bridge/Bid.hh"
#include "bridge/BridgeConstants.hh"
#include "bridge/Contract.hh"
#include "bridge/Partnership.hh"
#include "bridge/Position.hh"
#include "bridge/Vulnerability.hh"
#include "engine/DuplicateGameManager.hh"
#include "scoring/DuplicateScoring.hh"
#include "Enumerate.hh"

#include <gtest/gtest.h>

#include <tuple>

using Bridge::Engine::DuplicateGameManager;
using Bridge::Scoring::calculateDuplicateScore;
using Bridge::Position;

namespace {
constexpr Bridge::Partnership PARTNERSHIP {Bridge::Partnership::NORTH_SOUTH};
constexpr Bridge::Contract CONTRACT {
    Bridge::Bid {1, Bridge::Strain::CLUBS}, Bridge::Doubling::UNDOUBLED};
constexpr int TRICKS_WON {7};
}

class DuplicateGameManagerTest : public testing::Test
{
protected:
    DuplicateGameManager gameManager;
};

TEST_F(DuplicateGameManagerTest, testIsAlwaysOngoing)
{
    EXPECT_FALSE(gameManager.hasEnded());
}

TEST_F(DuplicateGameManagerTest, testInitiallyThereAreNoScoreEntries)
{
    EXPECT_EQ(0u, gameManager.getNumberOfEntries());
}

TEST_F(DuplicateGameManagerTest, testPassedOut)
{
    gameManager.addPassedOut();
    ASSERT_EQ(1u, gameManager.getNumberOfEntries());
    EXPECT_FALSE(gameManager.getScoreEntry(0));
}

TEST_F(DuplicateGameManagerTest, testAddResultContractMade)
{
    gameManager.addResult(PARTNERSHIP, CONTRACT, TRICKS_WON);
    ASSERT_EQ(1u, gameManager.getNumberOfEntries());
    const DuplicateGameManager::ScoreEntry entry {
        PARTNERSHIP,
        calculateDuplicateScore(CONTRACT, false, TRICKS_WON).second};
    EXPECT_EQ(entry, gameManager.getScoreEntry(0));
}

TEST_F(DuplicateGameManagerTest, testAddResultContractDefeated)
{
    gameManager.addResult(PARTNERSHIP, CONTRACT, TRICKS_WON - 1);
    ASSERT_EQ(1u, gameManager.getNumberOfEntries());
    const DuplicateGameManager::ScoreEntry entry {
        otherPartnership(PARTNERSHIP),
        calculateDuplicateScore(CONTRACT, false, TRICKS_WON - 1).second};
    EXPECT_EQ(entry, gameManager.getScoreEntry(0));
}

TEST_F(DuplicateGameManagerTest, testAddResultVulnerable)
{
    gameManager.addPassedOut();
    // North-south vulnerable
    gameManager.addResult(PARTNERSHIP, CONTRACT, TRICKS_WON - 1);
    ASSERT_EQ(2u, gameManager.getNumberOfEntries());
    const DuplicateGameManager::ScoreEntry entry {
        otherPartnership(PARTNERSHIP),
        calculateDuplicateScore(CONTRACT, true, TRICKS_WON - 1).second};
    EXPECT_EQ(entry, gameManager.getScoreEntry(1));
}

TEST_F(DuplicateGameManagerTest, testOpenerPositionRotation)
{
    for (int round = 0; round < 4; ++round) {
        for (const auto position : Bridge::POSITIONS) {
            EXPECT_EQ(position, gameManager.getOpenerPosition());
            gameManager.addResult(PARTNERSHIP, CONTRACT, TRICKS_WON);
        }
    }
}

TEST_F(DuplicateGameManagerTest, testVulnerabilityPositionRotation)
{
    //                  Position         NS     EW     vulnerable
    const std::array<std::tuple<Position, bool, bool>, 16u> rotation {
        std::make_tuple(Position::NORTH, false, false),
        std::make_tuple(Position::EAST,  true,  false),
        std::make_tuple(Position::SOUTH, false, true ),
        std::make_tuple(Position::WEST,  true,  true ),
        std::make_tuple(Position::NORTH, true,  false),
        std::make_tuple(Position::EAST,  false, true ),
        std::make_tuple(Position::SOUTH, true,  true ),
        std::make_tuple(Position::WEST,  false, false),
        std::make_tuple(Position::NORTH, false, true ),
        std::make_tuple(Position::EAST,  true,  true ),
        std::make_tuple(Position::SOUTH, false, false),
        std::make_tuple(Position::WEST,  true,  false),
        std::make_tuple(Position::NORTH, true,  true ),
        std::make_tuple(Position::EAST,  false, false),
        std::make_tuple(Position::SOUTH, true,  false),
        std::make_tuple(Position::WEST,  false, true ),
    };
    for (const auto e : Bridge::enumerate(rotation)) {
        EXPECT_EQ(std::get<0>(e.second), gameManager.getOpenerPosition());
        EXPECT_EQ(
            Bridge::Vulnerability(std::get<1>(e.second), std::get<2>(e.second)),
            gameManager.getVulnerability()) << "Round " << (e.first + 1);
        gameManager.addResult(PARTNERSHIP, CONTRACT, TRICKS_WON);
    }
}
