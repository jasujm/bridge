#include "bridge/Trick.hh"

#include "bridge/Card.hh"
#include "Utility.hh"

#include <boost/optional/optional.hpp>

#include <cassert>

namespace Bridge {

Trick::~Trick() = default;

bool Trick::play(const Hand& hand, const Card& card)
{
    if (!card.isKnown()) {
        return false;
    }
    const auto hand_in_turn = getHandInTurn();
    if (hand_in_turn.get_ptr() != &hand ||
        !handleIsPlayAllowed(*hand_in_turn, card)) {
        return false;
    }
    handleAddCardToTrick(card);
    return true;
}

boost::optional<const Hand&> Trick::getHandInTurn() const
{
    const auto n = internalGetNumberOfCardsPlayed();
    if (n == N_CARDS_IN_TRICK) {
        return boost::none;
    }
    return handleGetHand(n);
}

boost::optional<const Hand&> Trick::getWinner() const
{
    if (isCompleted()) {
        return handleGetWinner();
    }
    return boost::none;
}

boost::optional<const Card&> Trick::getCard(const Hand& hand) const
{
    // It is assumed that this is reasonably efficient:
    // - Only small number of iterations
    // - Calls to getters are expected to be fast
    const auto n = internalGetNumberOfCardsPlayed();
    for (const auto i : to(n)) {
        if (&hand == &handleGetHand(i)) {
            return handleGetCard(i);
        }
    }
    return boost::none;
}

bool Trick::isCompleted() const
{
    return (internalGetNumberOfCardsPlayed() == N_CARDS_IN_TRICK);
}

std::size_t Trick::internalGetNumberOfCardsPlayed() const
{
    const auto n = handleGetNumberOfCardsPlayed();
    assert(n <= N_CARDS_IN_TRICK);
    return n;
}

}
