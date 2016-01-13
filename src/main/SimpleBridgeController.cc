#include "main/SimpleBridgeController.hh"

#include "bridge/DealState.hh"
#include "bridge/Hand.hh"
#include "engine/BridgeEngine.hh"
#include "engine/CardManager.hh"
#include "engine/MakeDealState.hh"

#include <algorithm>

namespace Bridge {
namespace Main {

DealState SimpleBridgeController::getState() const
{
    assert(engine);
    return makeDealState(*engine);
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
