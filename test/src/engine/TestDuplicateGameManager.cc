#include "bridge/Bid.hh"
#include "bridge/BridgeConstants.hh"
#include "bridge/Contract.hh"
#include "bridge/Partnership.hh"
#include "bridge/Position.hh"
#include "bridge/Vulnerability.hh"
#include "engine/DuplicateGameManager.hh"
#include "scoring/DuplicateScoring.hh"
#include "Utility.hh"

#include <gtest/gtest.h>

using Bridge::Engine::DuplicateGameManager;
using Bridge::Scoring::calculateDuplicateScore;
using Bridge::to;

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
    auto offset = 0u;
    for (int round = 0; round < 4; ++round) {
        for (const auto i : to(Bridge::N_PLAYERS)) {
            EXPECT_EQ(
                Bridge::VULNERABILITIES[
                    (i + offset) % Bridge::N_VULNERABILITIES],
                gameManager.getVulnerability());
            gameManager.addResult(PARTNERSHIP, CONTRACT, TRICKS_WON);
        }
        ++offset;
    }
}
