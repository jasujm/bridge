#include "bridge/Bid.hh"
#include "bridge/BridgeConstants.hh"
#include "bridge/Contract.hh"
#include "bridge/DuplicateScoring.hh"
#include "bridge/Partnership.hh"
#include "bridge/Position.hh"
#include "bridge/Vulnerability.hh"
#include "engine/DuplicateGameManager.hh"
#include "Enumerate.hh"

#include <gtest/gtest.h>

#include <optional>
#include <tuple>

using Bridge::Engine::DuplicateGameManager;
using Bridge::Vulnerability;
using Bridge::DuplicateResult;
namespace Positions = Bridge::Positions;

namespace {

constexpr auto PARTNERSHIP = Bridge::Partnership {
    Bridge::Partnerships::NORTH_SOUTH};
constexpr auto CONTRACT = Bridge::Contract {
    Bridge::Bid {1, Bridge::Strains::CLUBS}, Bridge::Doublings::UNDOUBLED};
constexpr auto TRICKS_WON = 7;

constexpr auto DUPLICATE_DEAL_CONFIGS = std::array {
    std::pair { Positions::NORTH, Vulnerability {false, false} },
    std::pair { Positions::EAST,  Vulnerability {true, false}  },
    std::pair { Positions::SOUTH, Vulnerability {false, true}  },
    std::pair { Positions::WEST,  Vulnerability {true, true}   },
    std::pair { Positions::NORTH, Vulnerability {true, false}  },
    std::pair { Positions::EAST,  Vulnerability {false, true}  },
    std::pair { Positions::SOUTH, Vulnerability {true, true}   },
    std::pair { Positions::WEST,  Vulnerability {false, false} },
    std::pair { Positions::NORTH, Vulnerability {false, true}  },
    std::pair { Positions::EAST,  Vulnerability {true, true}   },
    std::pair { Positions::SOUTH, Vulnerability {false, false} },
    std::pair { Positions::WEST,  Vulnerability {true, false}  },
    std::pair { Positions::NORTH, Vulnerability {true, true}   },
    std::pair { Positions::EAST,  Vulnerability {false, false} },
    std::pair { Positions::SOUTH, Vulnerability {true, false}  },
    std::pair { Positions::WEST,  Vulnerability {false, true}  },
};

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
    EXPECT_EQ(
        DuplicateResult(),
        std::experimental::any_cast<DuplicateResult>(result));
}

TEST_F(DuplicateGameManagerTest, testAddResult)
{
    const auto expected_result = DuplicateResult {
        PARTNERSHIP,
        Bridge::calculateDuplicateScore(CONTRACT, false, TRICKS_WON)};
    const auto result = gameManager.addResult(
        PARTNERSHIP, CONTRACT, TRICKS_WON);
    EXPECT_EQ(
        expected_result, std::experimental::any_cast<DuplicateResult>(result));
}

TEST_F(DuplicateGameManagerTest, testVulnerabilityPositionRotation)
{
    for (const auto& config : DUPLICATE_DEAL_CONFIGS) {
        EXPECT_EQ(config.first, gameManager.getOpenerPosition());
        EXPECT_EQ(config.second, gameManager.getVulnerability());
        gameManager.addResult(PARTNERSHIP, CONTRACT, TRICKS_WON);
    }
}

TEST_F(DuplicateGameManagerTest, testConstructFromOpenerAndVulnerability)
{
    for (const auto& config : DUPLICATE_DEAL_CONFIGS) {
        const auto game_manager = DuplicateGameManager {
            config.first, config.second};
        EXPECT_EQ(config.first, game_manager.getOpenerPosition());
        EXPECT_EQ(config.second, game_manager.getVulnerability());
    }
}
