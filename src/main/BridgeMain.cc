#include "main/BridgeMain.hh"

#include "bridge/GameState.hh"
#include "engine/BridgeEngine.hh"
#include "engine/CardManager.hh"
#include "bridge/Hand.hh"
#include "engine/MakeGameState.hh"

#include <algorithm>

namespace Bridge {
namespace Main {

void BridgeMain::handleCall(const Call& call)
{
    if (const auto player = internalGetPlayer()) {
        assert(engine);
        engine->call(*player, call);
    }
}

void BridgeMain::handlePlay(const CardType& cardType)
{
    if (const auto player = internalGetPlayer()) {
        assert(engine);
        if (const auto hand = engine->getHand(*player)) {
            if (const auto n_card = findFromHand(*hand, cardType)) {
                engine->play(*player, *n_card);
            }
        }
    }
}

GameState BridgeMain::handleGetState() const
{
    assert(engine);
    return makeGameState(*engine);
}

Player* BridgeMain::internalGetPlayer()
{
    assert(engine);
    if (const auto player_in_turn = engine->getPlayerInTurn()) {
        const auto iter = std::find_if(
            players.begin(), players.end(),
            [player_in_turn](const auto& player_ptr)
            {
                return player_ptr.get() == player_in_turn;
            });
        if (iter != players.end()) {
            return iter->get();
        }
    }
    return nullptr;
}

}
}
