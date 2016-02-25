#include "bridge/Trick.hh"

#include "bridge/Card.hh"
#include "Utility.hh"

#include <cassert>

namespace Bridge {

Trick::~Trick() = default;

bool Trick::play(const Hand& hand, const Card& card)
{
    if (canPlay(hand, card)) {
        handleAddCardToTrick(card);
        return true;
    }
    return false;
}

bool Trick::canPlay(const Hand& hand, const Card& card) const
{
    if (card.isKnown()) {
        const auto hand_in_turn = getHandInTurn();
        return hand_in_turn == &hand &&
            handleIsPlayAllowed(*hand_in_turn, card);
    }
    return false;
}

const Hand* Trick::getHandInTurn() const
{
    const auto n = internalGetNumberOfCardsPlayed();
    if (n == N_CARDS_IN_TRICK) {
        return nullptr;
    }
    return &handleGetHand(n);
}

const Hand* Trick::getWinner() const
{
    if (isCompleted()) {
        return &handleGetWinner();
    }
    return nullptr;
}

const Card* Trick::getCard(const Hand& hand) const
{
    // It is assumed that this is reasonably efficient:
    // - Only small number of iterations
    // - Calls to getters are expected to be fast
    const auto n = internalGetNumberOfCardsPlayed();
    for (const auto i : to(n)) {
        if (&hand == &handleGetHand(i)) {
            return &handleGetCard(i);
        }
    }
    return nullptr;
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
