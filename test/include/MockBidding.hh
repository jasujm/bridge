#ifndef MOCKCONTRACT_HH_
#define MOCKCONTRACT_HH_

#include "bridge/Bidding.hh"
#include "bridge/Call.hh"
#include "bridge/Position.hh"

#include <gmock/gmock.h>

namespace Bridge {

class MockBidding : public Bidding
{
public:
    MOCK_METHOD1(handleAddCall, void(const Call&));
    MOCK_CONST_METHOD0(handleGetNumberOfCalls, int());
    MOCK_CONST_METHOD0(handleGetOpeningPosition, Position());
    MOCK_CONST_METHOD1(handleGetCall, Call(int));
};

}
#endif // MOCKCONTRACT_HH_
