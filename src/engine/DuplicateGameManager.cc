#include "engine/DuplicateGameManager.hh"

#include "bridge/Position.hh"
#include "bridge/Vulnerability.hh"
#include "scoring/DuplicateScoring.hh"

namespace Bridge {
namespace Engine {

namespace {

Vulnerability getVulnerabilityHelper(const std::size_t n)
{
    // Skip one vulnerability after each four deals
    const auto offset = n + n / N_VULNERABILITIES;
    return VULNERABILITIES[offset % N_VULNERABILITIES];
}

}

std::size_t DuplicateGameManager::getNumberOfEntries() const
{
    return entries.size();
}

boost::optional<DuplicateGameManager::ScoreEntry>
DuplicateGameManager::getScoreEntry(const std::size_t n) const
{
    return entries.at(n);
}

void DuplicateGameManager::handleAddResult(
    Bridge::Partnership partnership, const Bridge::Contract& contract,
    const int tricksWon)
{
    const auto vulnerability = getVulnerabilityHelper(entries.size());
    auto score = Scoring::calculateDuplicateScore(
        contract, isVulnerable(vulnerability, partnership), tricksWon);
    if (!score.first) {
        partnership = otherPartnership(partnership);
    }
    entries.emplace_back(ScoreEntry {partnership, score.second});
}

void DuplicateGameManager::handleAddPassedOut()
{
    entries.emplace_back(boost::none);
}

bool DuplicateGameManager::handleHasEnded() const
{
    return false;
}

Position DuplicateGameManager::handleGetOpenerPosition() const
{
    return POSITIONS[entries.size() % N_POSITIONS];
}

Vulnerability DuplicateGameManager::handleGetVulnerability() const
{
    return getVulnerabilityHelper(entries.size());
}

bool operator==(const DuplicateGameManager::ScoreEntry& lhs,
                const DuplicateGameManager::ScoreEntry& rhs)
{
    return &lhs == &rhs || (
        lhs.partnership == rhs.partnership && lhs.score == rhs.score);
}

}
}
