#ifndef MOCKCONTRACT_HH_
#define MOCKCONTRACT_HH_

#include "bridge/Bidding.hh"
#include "bridge/Call.hh"
#include "bridge/Player.hh"
#include "bridge/Position.hh"

#include <gmock/gmock.h>

namespace Bridge {

class MockBidding : public Bidding
{
public:
    MOCK_METHOD1(handleAddCall, void(const Call&));
    MOCK_CONST_METHOD0(handleGetNumberOfCalls, std::size_t());
    MOCK_CONST_METHOD0(handleGetOpeningPosition, Position());
    MOCK_CONST_METHOD1(handleGetCall, Call(std::size_t));
    MOCK_CONST_METHOD1(handleIsCallAllowed, bool(const Call&));
    MOCK_CONST_METHOD0(handleGetLowestAllowedBid, std::optional<Bid>());
    MOCK_CONST_METHOD0(handleGetContract, Contract());
    MOCK_CONST_METHOD0(handleGetDeclarerPosition, Position());
    MOCK_CONST_METHOD0(handleHasEnded, bool());
    MOCK_CONST_METHOD0(handleHasContract, bool());
};

}
#endif // MOCKCONTRACT_HH_
