#include "bridge/Bid.hh"
#include "bridge/BridgeConstants.hh"
#include "bridge/Contract.hh"
#include "bridge/Partnership.hh"
#include "bridge/Position.hh"
#include "bridge/Vulnerability.hh"
#include "engine/DuplicateGameManager.hh"
#include "scoring/DuplicateScore.hh"
#include "scoring/DuplicateScoring.hh"
#include "Enumerate.hh"

#include <gtest/gtest.h>

#include <optional>
#include <tuple>

using Bridge::Scoring::DuplicateScore;
using ScoreEntry = Bridge::Engine::DuplicateGameManager::ScoreEntry;
namespace Positions = Bridge::Positions;

namespace {
constexpr Bridge::Partnership PARTNERSHIP {Bridge::Partnership::NORTH_SOUTH};
constexpr Bridge::Contract CONTRACT {
    Bridge::Bid {1, Bridge::Strain::CLUBS}, Bridge::Doubling::UNDOUBLED};
constexpr int TRICKS_WON {7};
}

class DuplicateGameManagerTest : public testing::Test
{
protected:
    Bridge::Engine::DuplicateGameManager gameManager;
};

TEST_F(DuplicateGameManagerTest, testIsAlwaysOngoing)
{
    EXPECT_FALSE(gameManager.hasEnded());
}

TEST_F(DuplicateGameManagerTest, testAddPassedOut)
{
    const auto result = gameManager.addPassedOut();
    EXPECT_EQ(std::nullopt, std::experimental::any_cast<ScoreEntry>(result));
}

TEST_F(DuplicateGameManagerTest, testAddResult)
{
    const auto result = gameManager.addResult(
        PARTNERSHIP, CONTRACT, TRICKS_WON);
    EXPECT_EQ(
        Bridge::Scoring::calculateDuplicateScore(
            PARTNERSHIP, CONTRACT, false, TRICKS_WON),
        std::experimental::any_cast<ScoreEntry>(result));
}

TEST_F(DuplicateGameManagerTest, testVulnerabilityPositionRotation)
{
    //                  Position          NS     EW     vulnerable
    constexpr auto rotation = std::array {
        std::tuple {Positions::NORTH, false, false},
        std::tuple {Positions::EAST,  true,  false},
        std::tuple {Positions::SOUTH, false, true },
        std::tuple {Positions::WEST,  true,  true },
        std::tuple {Positions::NORTH, true,  false},
        std::tuple {Positions::EAST,  false, true },
        std::tuple {Positions::SOUTH, true,  true },
        std::tuple {Positions::WEST,  false, false},
        std::tuple {Positions::NORTH, false, true },
        std::tuple {Positions::EAST,  true,  true },
        std::tuple {Positions::SOUTH, false, false},
        std::tuple {Positions::WEST,  true,  false},
        std::tuple {Positions::NORTH, true,  true },
        std::tuple {Positions::EAST,  false, false},
        std::tuple {Positions::SOUTH, true,  false},
        std::tuple {Positions::WEST,  false, true },
    };
    for (const auto e : Bridge::enumerate(rotation)) {
        EXPECT_EQ(std::get<0>(e.second), gameManager.getOpenerPosition());
        EXPECT_EQ(
            Bridge::Vulnerability(std::get<1>(e.second), std::get<2>(e.second)),
            gameManager.getVulnerability()) << "Round " << (e.first + 1);
        gameManager.addResult(PARTNERSHIP, CONTRACT, TRICKS_WON);
    }
}
