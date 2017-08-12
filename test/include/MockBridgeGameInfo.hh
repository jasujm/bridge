#ifndef MOCKBRIDGEGAMEINFO_HH_
#define MOCKBRIDGEGAMEINFO_HH_

#include "main/BridgeGameInfo.hh"

#include <gmock/gmock.h>

namespace Bridge {
namespace Main {

class MockBridgeGameInfo : public BridgeGameInfo {
public:
    MOCK_CONST_METHOD0(handleGetEngine, const Engine::BridgeEngine&());
    MOCK_CONST_METHOD0(
        handleGetGameManager, const Engine::DuplicateGameManager&());
};

}
}

#endif // MOCKBRIDGEGAMEINFO_HH_
