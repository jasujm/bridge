#include "engine/DuplicateGameManager.hh"

#include "bridge/Position.hh"
#include "bridge/Vulnerability.hh"
#include "scoring/DuplicateScore.hh"
#include "scoring/DuplicateScoring.hh"


namespace Bridge {
namespace Engine {

namespace {

Vulnerability getVulnerabilityHelper(const int round)
{
    // Skip one vulnerability after each four deals
    const auto offset = (round + round / Position::size()) % Position::size();
    return {offset == 1 || offset == 3, offset == 2 || offset == 3};
}

}

DuplicateGameManager::DuplicateGameManager() :
    round {0}
{
}

GameManager::ResultType DuplicateGameManager::handleAddResult(
    const Partnership partnership, const Contract& contract,
    const int tricksWon)
{
    const auto vulnerability = getVulnerabilityHelper(round);
    ++round;
    return ScoreEntry {
        Scoring::calculateDuplicateScore(
            partnership, contract, isVulnerable(vulnerability, partnership),
            tricksWon)};
}

GameManager::ResultType DuplicateGameManager::handleAddPassedOut()
{
    ++round;
    return ScoreEntry {};
}

bool DuplicateGameManager::handleHasEnded() const
{
    return false;
}

Position DuplicateGameManager::handleGetOpenerPosition() const
{
    return static_cast<PositionLabel>(round % Position::size());
}

Vulnerability DuplicateGameManager::handleGetVulnerability() const
{
    return getVulnerabilityHelper(round);
}

}
}
