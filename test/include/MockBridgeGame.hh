#ifndef MOCKBRIDGEGAME_HH_
#define MOCKBRIDGEGAME_HH_

#include "bridge/BridgeGame.hh"
#include "bridge/CardType.hh"
#include "bridge/GameState.hh"

#include <gmock/gmock.h>

namespace Bridge {

class MockBridgeGame : public BridgeGame
{
public:
    MOCK_METHOD1(handleCall, void(const Call&));
    MOCK_METHOD1(handlePlay, void(const CardType&));
    MOCK_CONST_METHOD0(handleGetState, GameState());
};


}

#endif // MOCKBRIDGEGAME_HH_
