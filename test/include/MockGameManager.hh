#ifndef MOCKGAMEMANAGER_HH_
#define MOCKGAMEMANAGER_HH_

#include "bridge/Contract.hh"
#include "bridge/Partnership.hh"
#include "bridge/Position.hh"
#include "bridge/Vulnerability.hh"
#include "engine/GameManager.hh"

#include <gmock/gmock.h>

namespace Bridge {
namespace Engine {

class MockGameManager : public GameManager
{
public:
    MOCK_METHOD3(handleAddResult, ResultType(Partnership, const Contract&, int));
    MOCK_METHOD0(handleAddPassedOut, ResultType());
    MOCK_CONST_METHOD0(handleHasEnded, bool());
    MOCK_CONST_METHOD0(handleGetOpenerPosition, Position());
    MOCK_CONST_METHOD0(handleGetVulnerability, Vulnerability());
};

}
}

#endif // MOCKGAMEMANAGER_HH_
