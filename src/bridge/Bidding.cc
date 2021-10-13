#include "bridge/Bidding.hh"

#include "bridge/BridgeConstants.hh"
#include "bridge/Position.hh"
#include "Utility.hh"

#include <boost/logic/tribool.hpp>

namespace Bridge {

namespace {

class CallAllowedVisitor {
public:
    CallAllowedVisitor(const Bidding& bidding) :
        bidding {bidding}
    {
    }

    bool operator()(Pass) const
    {
        return true;
    }

    bool operator()(const Bid& bid) const
    {
        return bid >= dereference(bidding.getLowestAllowedBid());
    }

    bool operator()(Double) const
    {
        return bidding.isDoublingAllowed();
    }

    bool operator()(Redouble) const
    {
        return bidding.isRedoublingAllowed();
    }

private:
    const Bidding& bidding;
};

}

Bidding::~Bidding() = default;

bool Bidding::call(const Position position, const Call& call)
{
    if (position == getPositionInTurn()) {
        if (std::visit(CallAllowedVisitor {*this}, call)) {
            handleAddCall(call);
            return true;
        }
    }
    return false;
}

std::optional<Bid> Bidding::getLowestAllowedBid() const
{
    if (hasEnded()) {
        return std::nullopt;
    }
    const auto n_calls = handleGetNumberOfCalls();
    for (const auto n : to(n_calls) | std::ranges::views::reverse) {
        const auto call = handleGetCall(n);
        if (const auto bid = std::get_if<Bid>(&call)) {
            return nextHigherBid(*bid);
        }
    }
    return Bid::LOWEST_BID;
}

bool Bidding::isDoublingAllowed() const
{
    if (hasEnded()) {
        return false;
    }
    const auto n_calls = handleGetNumberOfCalls();
    for (const auto n : to(n_calls) | std::ranges::views::reverse) {
        const auto call = handleGetCall(n);
        // Opponent bid last
        if (std::holds_alternative<Bid>(call)) {
            return (n_calls - n) % 2 == 1;
        }
        // Doubling not allowed if the last call was double or redouble
        else if (!std::holds_alternative<Pass>(call)) {
            return false;
        }
    }
    return false;
}

bool Bidding::isRedoublingAllowed() const
{
    if (hasEnded()) {
        return false;
    }
    const auto n_calls = handleGetNumberOfCalls();
    for (const auto n : to(n_calls) | std::ranges::views::reverse) {
        const auto call = handleGetCall(n);
        // Opponent doubled last
        if (std::holds_alternative<Double>(call)) {
            return (n_calls - n) % 2 == 1;
        }
        // Redoubling not allowed if the last call was bid or redouble
        else if (!std::holds_alternative<Pass>(call)) {
            return false;
        }
    }
    return false;
}

std::optional<Position> Bidding::getPositionInTurn() const
{
    if (!hasEnded()) {
        const auto opener = handleGetOpeningPosition();
        const auto steps = getNumberOfCalls();
        return clockwise(opener, steps);
    }
    return std::nullopt;
}

int Bidding::getNumberOfCalls() const
{
    return handleGetNumberOfCalls();
}

Position Bidding::getOpeningPosition() const
{
    return handleGetOpeningPosition();
}

Call Bidding::getCall(const int n) const
{
    const auto n_calls = getNumberOfCalls();
    return handleGetCall(checkIndex(n, n_calls));
}

boost::logic::tribool Bidding::hasContract() const
{
    if (hasEnded()) {
        for (const auto n : to(handleGetNumberOfCalls())) {
            const auto call = handleGetCall(n);
            if (std::holds_alternative<Bid>(call)) {
                return true;
            }
        }
        return false;
    }
    return boost::logic::indeterminate;
}

std::optional<std::optional<Contract>> Bidding::getContract() const
{
    return internalGetIfHasContract(&Bidding::internalGetContract);
}

std::optional<std::optional<Position>> Bidding::getDeclarerPosition() const
{
    return internalGetIfHasContract(&Bidding::internalGetDeclarerPosition);
}

bool Bidding::hasEnded() const
{
    const auto n_calls = handleGetNumberOfCalls();
    if (n_calls < N_PLAYERS) {
        return false;
    }
    constexpr auto n_passes_required = N_PLAYERS - 1;
    for (const auto n : from_to(n_calls - n_passes_required, n_calls)) {
        const auto call = handleGetCall(n);
        if (!std::holds_alternative<Pass>(call)) {
            return false;
        }
    }
    return true;
}

template<class T>
std::optional<T> Bidding::internalGetIfHasContract(
    T (Bidding::*const memfn)() const) const
{
    if (hasEnded()) {
        return (this->*memfn)();
    }
    return std::nullopt;
}

std::optional<Contract> Bidding::internalGetContract() const
{
    auto doubling = Doublings::UNDOUBLED;
    const auto n_calls = handleGetNumberOfCalls();
    for (const auto n : to(n_calls) | std::ranges::views::reverse) {
        const auto call = handleGetCall(n);
        if (std::holds_alternative<Double>(call)) {
            // Examining the calls from the last to the first, so
            // ignore the double if we already saw a redouble
            if (doubling == Doublings::UNDOUBLED) {
                doubling = Doublings::DOUBLED;
            }
        } else if (std::holds_alternative<Redouble>(call)) {
            doubling = Doublings::REDOUBLED;
        } else if (const auto bid = std::get_if<Bid>(&call)) {
            return Contract {*bid, doubling};
        }
    }
    return std::nullopt;
}

std::optional<Position> Bidding::internalGetDeclarerPosition() const
{
    const auto n_calls = handleGetNumberOfCalls();
    for (const auto n : to(n_calls) | std::ranges::views::reverse) {
        const auto call = handleGetCall(n);
        if (const auto bid = std::get_if<Bid>(&call)) {
            // Find the first bid by the contract partnership that has
            // the same strain as the contract bid. The player who
            // made it is the declarer.
            for (const auto n2 : from_to(n % 2, n_calls, 2)) {
                const auto call2 = handleGetCall(n2);
                if (const auto bid2 = std::get_if<Bid>(&call2)) {
                    if (bid->strain == bid2->strain) {
                        return clockwise(handleGetOpeningPosition(), n2);
                    }
                }
            }
        }
    }
    return std::nullopt;
}

}
