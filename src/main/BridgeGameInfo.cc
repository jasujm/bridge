#include "main/BridgeGameInfo.hh"

namespace Bridge {
namespace Main {

BridgeGameInfo::~BridgeGameInfo() = default;

const Engine::BridgeEngine& BridgeGameInfo::getEngine() const
{
    return handleGetEngine();
}

const Engine::DuplicateGameManager& BridgeGameInfo::getGameManager() const
{
    return handleGetGameManager();
}

}
}
