#include "bridge/BasicBidding.hh"

#include "bridge/Bid.hh"
#include "bridge/BridgeConstants.hh"
#include "bridge/Contract.hh"
#include "bridge/Position.hh"
#include "Utility.hh"

#include <algorithm>
#include <cassert>
#include <tuple>
#include <utility>

namespace Bridge {

BasicBidding::BasicBidding(const Position openingPosition) :
    openingPosition {openingPosition}
{
}

void BasicBidding::handleAddCall(const Call &call)
{
    calls.emplace_back(call);
}

int BasicBidding::handleGetNumberOfCalls() const
{
    return calls.size();
}

Position BasicBidding::handleGetOpeningPosition() const
{
    return openingPosition;
}

Call BasicBidding::handleGetCall(const int n) const
{
    assert(0 <= n && n < ssize(calls));
    return calls[n];
}

}
