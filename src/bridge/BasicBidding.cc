#include "bridge/BasicBidding.hh"

#include "bridge/Bid.hh"
#include "bridge/BridgeConstants.hh"
#include "bridge/Contract.hh"
#include "bridge/Position.hh"

#include <algorithm>
#include <cassert>
#include <tuple>
#include <utility>

using boost::logic::tribool;

namespace Bridge {

namespace {

using BiddingState = std::pair<std::optional<Contract>, tribool>;

// Visitor for determining how adding a call affects the contract-in-making
class AddCallVisitor {
public:
    AddCallVisitor(const std::optional<Contract>& contract,
                   tribool lastBidderHasTurn) :
        contract {contract},
        lastBidderHasTurn {lastBidderHasTurn}
    {
    }

    BiddingState operator()(const Bid& bid) const
    {
        return {Contract {bid, Doubling::UNDOUBLED}, false};
    }

    BiddingState operator()(Pass) const
    {
        return {contract, !lastBidderHasTurn};
    }

    BiddingState operator()(Double) const
    {
        assert(contract);
        return {Contract {contract->bid, Doubling::DOUBLED},
                !lastBidderHasTurn};
    }

    BiddingState operator()(Redouble) const
    {
        assert(contract);
        return {Contract {contract->bid, Doubling::REDOUBLED},
                !lastBidderHasTurn};
    }

private:
    const std::optional<Contract>& contract;
    tribool lastBidderHasTurn;
};

// Visitor for determining if call is allowed
class CallAllowedVisitor {
public:
    CallAllowedVisitor(const std::optional<Contract>& contract,
                       const tribool& lastBidderHasTurn) :
        contract {contract},
        lastBidderHasTurn {lastBidderHasTurn}
    {
    }

    bool operator()(const Bid& bid) const
    {
        return !contract || bid > contract->bid;
    }

    bool operator()(Pass) const
    {
        return true;
    }

    bool operator()(Double) const
    {
        return bool(!lastBidderHasTurn) &&
            contract->doubling == Doubling::UNDOUBLED;
    }

    bool operator()(Redouble) const
    {
        return bool(lastBidderHasTurn) &&
            contract->doubling == Doubling::DOUBLED;
    }

private:
    const std::optional<Contract>& contract;
    const tribool lastBidderHasTurn;
};

// Visitor for determining whether the given call is a bid with given strain
class StrainBidVisitor {
public:
    StrainBidVisitor(Strain strain) :
        strain {strain}
    {
    }

    bool operator()(const Bid& bid) const
    {
        return bid.strain == strain;
    }

    template<typename T>
    bool operator()(const T&) const
    {
        return false;
    }

private:
    const Strain strain;
};

}

BasicBidding::BasicBidding(const Position openingPosition) :
    openingPosition {openingPosition},
    lastBidderHasTurn {boost::logic::indeterminate}
{
}

void BasicBidding::handleAddCall(const Call &call)
{
    assert(bool(contract) == !indeterminate(lastBidderHasTurn));
    auto new_state = boost::apply_visitor(
        AddCallVisitor {contract, lastBidderHasTurn}, call);
    calls.emplace_back(call);

    // Strong expection guarantee, only update state after call has been added
    std::tie(contract, lastBidderHasTurn) = std::move(new_state);
}

std::size_t BasicBidding::handleGetNumberOfCalls() const
{
    return calls.size();
}

Position BasicBidding::handleGetOpeningPosition() const
{
    return openingPosition;
}

Call BasicBidding::handleGetCall(const std::size_t n) const
{
    assert(n < calls.size());
    return calls[n];
}

bool BasicBidding::handleIsCallAllowed(const Call& call) const
{
    assert(bool(contract) == !indeterminate(lastBidderHasTurn));
    return boost::apply_visitor(
        CallAllowedVisitor {contract, lastBidderHasTurn}, call);
}

std::optional<Bid> BasicBidding::handleGetLowestAllowedBid() const
{
    if (contract) {
        return nextHigherBid(contract->bid);
    }
    return Bid::LOWEST_BID;
}

Contract BasicBidding::handleGetContract() const
{
    assert(contract);
    return *contract;
}

Position BasicBidding::handleGetDeclarerPosition() const
{
    assert(contract);
    StrainBidVisitor visitor {contract->bid.strain};
    // If contract is doubled, the last call was made by declarer's opponent,
    // otherwise by declarer's partnership.
    const auto offset = (contract->doubling == Doubling::DOUBLED);
    auto i = (calls.size() + offset) % 2;
    while(true) {
        if (boost::apply_visitor(visitor, calls.at(i))) {
            return clockwise(openingPosition, i);
        }
        i += 2;
    }
}

bool BasicBidding::handleHasEnded() const
{
    if (calls.size() < N_PLAYERS) {
        return false;
    }

    constexpr auto n_passes_required = N_PLAYERS - 1;
    const auto n_passes = std::count(
        calls.rbegin(), calls.rbegin() + n_passes_required, Call {Pass {}});
    return n_passes == n_passes_required;
}

bool BasicBidding::handleHasContract() const
{
    return bool(contract);
}

}
