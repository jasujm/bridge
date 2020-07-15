#ifndef MOCKDEAL_HH_
#define MOCKDEAL_HH_

#include "bridge/Bidding.hh"
#include "bridge/Deal.hh"
#include "bridge/Hand.hh"
#include "bridge/Trick.hh"
#include "bridge/Vulnerability.hh"

#include <gmock/gmock.h>

namespace Bridge {

class MockDeal : public Deal {
public:
    MOCK_CONST_METHOD0(handleGetUuid, const Uuid&());
    MOCK_CONST_METHOD0(handleGetPhase, DealPhase());
    MOCK_CONST_METHOD0(handleGetVulnerability, Vulnerability());
    MOCK_CONST_METHOD0(handleGetPositionInTurn, Position());
    MOCK_CONST_METHOD1(handleGetHand, const Hand&(Position));
    MOCK_CONST_METHOD0(handleGetBidding, const Bidding&());
    MOCK_CONST_METHOD0(handleGetNumberOfTricks, int());
    MOCK_CONST_METHOD1(handleGetTrick, const Trick&(int));
    MOCK_CONST_METHOD0(handleGetCurrentTrick, const Trick&());
};

}


#endif // MOCKDEAL_HH_
