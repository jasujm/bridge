#include "bridge/BridgeGame.hh"

#include "bridge/GameState.hh"

namespace Bridge {

BridgeGame::~BridgeGame() = default;

void BridgeGame::call(const Call& call)
{
    handleCall(call);
}

void BridgeGame::play(const CardType& card)
{
    handlePlay(card);
}

GameState BridgeGame::getState() const
{
    return handleGetState();;
}

}
