#include "engine/GameManager.hh"

#include "bridge/Partnership.hh"
#include "bridge/Position.hh"
#include "bridge/Vulnerability.hh"

namespace Bridge {
namespace Engine {

GameManager::~GameManager() = default;

GameManager::ResultType GameManager::addResult(
    const Partnership partnership, const Contract& contract,
    const int tricksWon)
{
    if (!hasEnded()) {
        return handleAddResult(partnership, contract, tricksWon);
    }
    return {};
}

GameManager::ResultType GameManager::addPassedOut()
{
    if (!hasEnded()) {
        return handleAddPassedOut();
    }
    return {};
}

bool GameManager::hasEnded() const
{
    return handleHasEnded();
}

std::optional<Position> GameManager::getOpenerPosition() const
{
    if (!hasEnded()) {
        return handleGetOpenerPosition();
    }
    return std::nullopt;
}

std::optional<Vulnerability> GameManager::getVulnerability() const
{
    if (!hasEnded()) {
        return handleGetVulnerability();
    }
    return std::nullopt;
}

}
}
