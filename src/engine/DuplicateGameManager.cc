#include "engine/DuplicateGameManager.hh"

#include "bridge/DuplicateScoring.hh"
#include "bridge/Position.hh"
#include "bridge/Vulnerability.hh"

#include <array>
#include <algorithm>

namespace Bridge {
namespace Engine {

namespace {

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

Vulnerability getVulnerabilityHelper(const int round)
{
    return DUPLICATE_DEAL_CONFIGS[round % DUPLICATE_DEAL_CONFIGS.size()].second;
}

}

DuplicateGameManager::DuplicateGameManager() :
    round {0}
{
}

DuplicateGameManager::DuplicateGameManager(
    const Position openerPosition, const Vulnerability vulnerability)
{
    const auto iter = std::find(
        DUPLICATE_DEAL_CONFIGS.begin(), DUPLICATE_DEAL_CONFIGS.end(),
        std::pair {openerPosition, vulnerability});
    if (iter == DUPLICATE_DEAL_CONFIGS.end()) {
        throw std::invalid_argument {"Invalid position or vulnerability"};
    }
    round = iter - DUPLICATE_DEAL_CONFIGS.begin();
}

GameManager::ResultType DuplicateGameManager::handleAddResult(
    const Partnership partnership, const Contract& contract,
    const int tricksWon)
{
    const auto vulnerability = getVulnerabilityHelper(round);
    ++round;
    const auto score = calculateDuplicateScore(
        contract, isVulnerable(vulnerability, partnership), tricksWon);
    return makeDuplicateResult(partnership, score);
}

GameManager::ResultType DuplicateGameManager::handleAddPassedOut()
{
    ++round;
    return DuplicateResult::passedOut();
}

bool DuplicateGameManager::handleHasEnded() const
{
    return false;
}

Position DuplicateGameManager::handleGetOpenerPosition() const
{
    return DUPLICATE_DEAL_CONFIGS[round % DUPLICATE_DEAL_CONFIGS.size()].first;
}

Vulnerability DuplicateGameManager::handleGetVulnerability() const
{
    return getVulnerabilityHelper(round);
}

}
}
