#include "engine/GameManager.hh"

#include "bridge/Partnership.hh"
#include "bridge/Position.hh"
#include "bridge/Vulnerability.hh"

#include <boost/optional/optional.hpp>

namespace Bridge {
namespace Engine {

GameManager::~GameManager() = default;

void GameManager::addResult(
    const Partnership partnership, const Contract& contract,
    const int tricksWon)
{
    if (!hasEnded()) {
        handleAddResult(partnership, contract, tricksWon);
    }
}

void GameManager::addPassedOut()
{
    if (!hasEnded()) {
        handleAddPassedOut();
    }
}

bool GameManager::hasEnded() const
{
    return handleHasEnded();
}

boost::optional<Position> GameManager::getOpenerPosition() const
{
    if (!hasEnded()) {
        return handleGetOpenerPosition();
    }
    return boost::none;
}

boost::optional<Vulnerability> GameManager::getVulnerability() const
{
    if (!hasEnded()) {
        return handleGetVulnerability();
    }
    return boost::none;
}

}
}
