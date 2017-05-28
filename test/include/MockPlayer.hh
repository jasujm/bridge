#ifndef MOCKPLAYER_HH_
#define MOCKPLAYER_HH_

#include "bridge/Player.hh"

#include <gmock/gmock.h>

namespace Bridge {

class MockPlayer : public Player {
public:
    MOCK_CONST_METHOD0(handleGetUuid, Uuid());
};

}

#endif // MOCKPLAYER_HH_
