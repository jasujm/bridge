#include "engine/DuplicateGameManager.hh"

#include "bridge/Position.hh"
#include "bridge/Vulnerability.hh"

#include <iterator>

namespace Bridge {
namespace Engine {

namespace {

auto getNumberOfEntries(const Scoring::DuplicateScoreSheet& scoreSheet)
{
    return std::distance(scoreSheet.begin(), scoreSheet.end());
}

Vulnerability getVulnerabilityHelper(
    const Scoring::DuplicateScoreSheet& scoreSheet)
{
    // Skip one vulnerability after each four deals
    const auto n = getNumberOfEntries(scoreSheet);
    const auto offset = (n + n / N_POSITIONS) % N_POSITIONS;
    return {offset == 1 || offset == 3, offset == 2 || offset == 3};
}

}

const Scoring::DuplicateScoreSheet& DuplicateGameManager::getScoreSheet() const
{
    return scoreSheet;
}

boost::any DuplicateGameManager::handleAddResult(
    Partnership partnership, const Contract& contract, const int tricksWon)
{
    const auto vulnerability = getVulnerabilityHelper(scoreSheet);
    scoreSheet.addResult(partnership, contract, tricksWon, vulnerability);
    return {};
}

boost::any DuplicateGameManager::handleAddPassedOut()
{
    scoreSheet.addPassedOut();
    return {};
}

bool DuplicateGameManager::handleHasEnded() const
{
    return false;
}

Position DuplicateGameManager::handleGetOpenerPosition() const
{
    const auto n = getNumberOfEntries(scoreSheet);
    return POSITIONS[n % N_POSITIONS];
}

Vulnerability DuplicateGameManager::handleGetVulnerability() const
{
    return getVulnerabilityHelper(scoreSheet);
}

}
}
