#include "main/SimpleBridgeController.hh"

#include "bridge/GameState.hh"
#include "bridge/Hand.hh"
#include "engine/BridgeEngine.hh"
#include "engine/CardManager.hh"
#include "engine/MakeGameState.hh"

#include <algorithm>

namespace Bridge {
namespace Main {

GameState SimpleBridgeController::getState() const
{
    assert(engine);
    return makeGameState(*engine);
}

void SimpleBridgeController::handleCall(const Call& call)
{
    assert(engine);
    if (const auto player = engine->getPlayerInTurn()) {
        engine->call(*player, call);
    }
}

void SimpleBridgeController::handlePlay(const CardType& cardType)
{
    assert(engine);
    if (const auto player = engine->getPlayerInTurn()) {
        if (const auto hand = engine->getHand(*player)) {
            if (const auto n_card = findFromHand(*hand, cardType)) {
                engine->play(*player, *n_card);
            }
        }
    }
}

}
}
