#include "bridge/Deal.hh"

#include "bridge/Bidding.hh"
#include "bridge/Position.hh"
#include "bridge/Trick.hh"
#include "bridge/Vulnerability.hh"
#include "Utility.hh"

namespace Bridge {

const Hand& Deal::internalGetHandInTurn() const
{
    const auto& trick = dereference(getCurrentTrick());
    return dereference(trick.getHandInTurn());
}

const Position Deal::internalGetDeclarerPosition() const
{
    const auto& bidding = getBidding();
    return dereference(dereference(bidding.getDeclarerPosition()));
}

Deal::~Deal() = default;

const Uuid& Deal::getUuid() const
{
    return handleGetUuid();
}

DealPhase Deal::getPhase() const
{
    return handleGetPhase();
}

Vulnerability Deal::getVulnerability() const
{
    return handleGetVulnerability();
}

std::optional<Position> Deal::getPositionInTurn() const
{
    switch (getPhase()) {
    case DealPhase::BIDDING:
        return getBidding().getPositionInTurn();
    case DealPhase::PLAYING:
    {
        const auto position = getPosition(internalGetHandInTurn());
        const auto declarer_position = internalGetDeclarerPosition();
        return position == partnerFor(declarer_position) ?
            declarer_position : position;
    }
    case DealPhase::ENDED:
        break;
    }
    return std::nullopt;
}

const Hand* Deal::getHandInTurn() const
{
    if (getPhase() == DealPhase::PLAYING) {
        return &internalGetHandInTurn();
    }
    return nullptr;
}

const Hand& Deal::getHand(const Position position) const
{
    return handleGetHand(position);
}

std::optional<Position> Deal::getPosition(const Hand& hand) const
{
    for (const auto position : Position::all()) {
        if (&handleGetHand(position) == &hand) {
            return position;
        }
    }
    return std::nullopt;
}

bool Deal::isVisibleToAll(const Position position) const
{
    switch (getPhase()) {
    case DealPhase::BIDDING:
        break;
    case DealPhase::PLAYING:
    {
        // First check if there is a card in the opening lead. If
        // three is, the dummy position is visible.
        const auto& trick = getTrick(0).first.get();
        if (trick.begin() != trick.end()) {
            return partnerFor(position) == internalGetDeclarerPosition();
        }
        break;
    }
    case DealPhase::ENDED:
        return true;
    }
    return false;
}

const Bidding& Deal::getBidding() const
{
    return handleGetBidding();
}

int Deal::getNumberOfTricks() const
{
    return handleGetNumberOfTricks();
}

Deal::TrickPositionPair Deal::getTrick(const int n) const
{
    const auto n_tricks = getNumberOfTricks();
    return handleGetTrick(checkIndex(n, n_tricks));
}

const Trick* Deal::getCurrentTrick() const
{
    const auto n_tricks = getNumberOfTricks();
    if (n_tricks > 0) {
        return &handleGetTrick(n_tricks - 1).first.get();
    }
    return nullptr;
}

}
