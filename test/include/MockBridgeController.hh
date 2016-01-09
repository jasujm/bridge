#ifndef MOCKBRIDGEGAME_HH_
#define MOCKBRIDGEGAME_HH_

#include "bridge/CardType.hh"
#include "main/BridgeController.hh"

#include <gmock/gmock.h>

namespace Bridge {

class MockBridgeController : public BridgeController
{
public:
    MOCK_METHOD1(handleCall, void(const Call&));
    MOCK_METHOD1(handlePlay, void(const CardType&));
};


}

#endif // MOCKBRIDGEGAME_HH_
