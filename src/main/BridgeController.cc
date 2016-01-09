#include "main/BridgeController.hh"

namespace Bridge {

BridgeController::~BridgeController() = default;

void BridgeController::call(const Call& call)
{
    handleCall(call);
}

void BridgeController::play(const CardType& card)
{
    handlePlay(card);
}

}
