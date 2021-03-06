#include "bridge/Deal.hh"

#include "bridge/Bidding.hh"
#include "bridge/Contract.hh"
#include "bridge/Partnership.hh"
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

std::optional<Suit> Deal::internalGetTrump() const
{
    const auto& bidding = getBidding();
    const auto contract = dereference(dereference(bidding.getContract()));
    switch (contract.bid.strain.get()) {
    case StrainLabel::CLUBS:
        return Suits::CLUBS;
    case StrainLabel::DIAMONDS:
        return Suits::DIAMONDS;
    case StrainLabel::HEARTS:
        return Suits::HEARTS;
    case StrainLabel::SPADES:
        return Suits::SPADES;
    default:
        return std::nullopt;
    }
}

Position Deal::internalGetDeclarerPosition() const
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
    switch (getPhase().get()) {
    case DealPhaseLabel::BIDDING:
        return getBidding().getPositionInTurn();
    case DealPhaseLabel::PLAYING:
    {
        const auto position = getPosition(internalGetHandInTurn());
        const auto declarer_position = internalGetDeclarerPosition();
        return position == partnerFor(declarer_position) ?
            declarer_position : position;
    }
    case DealPhaseLabel::ENDED:
        break;
    }
    return std::nullopt;
}

const Hand* Deal::getHandInTurn() const
{
    if (getPhase() == DealPhases::PLAYING) {
        return &internalGetHandInTurn();
    }
    return nullptr;
}

const Hand& Deal::getHand(const Position position) const
{
    return handleGetHand(position);
}

const Card& Deal::getCard(const int n) const
{
    return handleGetCard(checkIndex(n, N_CARDS));
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
    switch (getPhase().get()) {
    case DealPhaseLabel::BIDDING:
        break;
    case DealPhaseLabel::PLAYING:
    {
        // First check if there is a card in the opening lead. If
        // three is, the dummy position is visible.
        const auto& trick = getTrick(0);
        if (trick.begin() != trick.end()) {
            return partnerFor(position) == internalGetDeclarerPosition();
        }
        break;
    }
    case DealPhaseLabel::ENDED:
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

const Trick& Deal::getTrick(const int n) const
{
    const auto n_tricks = getNumberOfTricks();
    return handleGetTrick(checkIndex(n, n_tricks));
}

std::optional<Position> Deal::getWinnerOfTrick(int n) const
{
    switch (getPhase().get()) {
    case DealPhaseLabel::BIDDING:
        break;
    case DealPhaseLabel::PLAYING:
    case DealPhaseLabel::ENDED:
    {
        const auto& trick = getTrick(n);
        if (const auto* hand = getWinner(trick, internalGetTrump())) {
            return getPosition(*hand);
        }
    }
    }
    return std::nullopt;
}

const Trick* Deal::getCurrentTrick() const
{
    const auto n_tricks = getNumberOfTricks();
    if (n_tricks > 0) {
        return &handleGetTrick(n_tricks - 1);
    }
    return nullptr;
}

std::optional<int> Deal::getTricksWonByDeclarer() const
{
    const auto n_tricks = getNumberOfTricks();
    if (n_tricks <= 0) {
        return std::nullopt;
    }
    auto tricks_won_by_declarer = 0;
    const auto declarer_position = internalGetDeclarerPosition();
    const auto declarer_partnership = partnershipFor(declarer_position);
    for (const auto n : to(n_tricks)) {
        if (const auto winner_position = getWinnerOfTrick(n)) {
            const auto winner_partnership = partnershipFor(*winner_position);
            if (winner_partnership == declarer_partnership) {
                ++tricks_won_by_declarer;
            }
        }
    }
    return tricks_won_by_declarer;
}

}
