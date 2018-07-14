#include "bridge/Bidding.hh"

#include "bridge/Position.hh"
#include "Utility.hh"

#include <boost/logic/tribool.hpp>

namespace Bridge {

Bidding::~Bidding() = default;

bool Bidding::call(const Position position, const Call& call)
{
    if (position == getPositionInTurn() && handleIsCallAllowed(call)) {
        handleAddCall(call);
        return true;
    }
    return false;
}

std::optional<Bid> Bidding::getLowestAllowedBid() const
{
    if (!hasEnded()) {
        return handleGetLowestAllowedBid();
    }
    return std::nullopt;
}

bool Bidding::isDoublingAllowed() const
{
    return !hasEnded() && handleIsCallAllowed(Call {Double {}});
}

bool Bidding::isRedoublingAllowed() const
{
    return !hasEnded() && handleIsCallAllowed(Call {Redouble {}});
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

std::size_t Bidding::getNumberOfCalls() const
{
    return handleGetNumberOfCalls();
}

Position Bidding::getOpeningPosition() const
{
    return handleGetOpeningPosition();
}

Call Bidding::getCall(const std::size_t n) const
{
    const auto n_calls = getNumberOfCalls();
    return handleGetCall(checkIndex(n, n_calls));
}

boost::logic::tribool Bidding::hasContract() const
{
    if (hasEnded()) {
        return handleHasContract();
    }
    return boost::logic::indeterminate;
}

std::optional<std::optional<Contract>> Bidding::getContract() const
{
    return internalGetIfHasContract(&Bidding::handleGetContract);
}

std::optional<std::optional<Position>> Bidding::getDeclarerPosition() const
{
    return internalGetIfHasContract(&Bidding::handleGetDeclarerPosition);
}

bool Bidding::hasEnded() const
{
    return handleHasEnded();
}

template<class T>
std::optional<std::optional<T>> Bidding::internalGetIfHasContract(
    T (Bidding::*const memfn)() const) const
{
    if (hasEnded()) {
        if (handleHasContract()) {
            return std::optional<T> {(this->*memfn)()};
        }
        return std::optional<T> {std::nullopt};
    }
    return std::nullopt;
}

}
