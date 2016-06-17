#include "bridge/Trick.hh"

#include "bridge/Card.hh"
#include "bridge/CardType.hh"
#include "bridge/Hand.hh"
#include "Utility.hh"

#include <cassert>
#include <boost/logic/tribool.hpp>

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
        const auto n = internalGetNumberOfCardsPlayed();
        if (n == 0) {
            return true;
        }
        const auto& hand_in_turn = handleGetHand(n);
        if (&hand_in_turn != &hand) {
            return false;
        }
        const auto suit = dereference(card.getType()).suit;
        const auto first_suit = dereference(handleGetCard(0).getType()).suit;
        return suit == first_suit || !bool(!hand.isOutOfSuit(first_suit));
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

const Hand* getWinner(const Trick& trick, const boost::optional<Suit> trump)
{
    if (!trick.isCompleted()) {
        return nullptr;
    }

    auto iter = trick.begin();
    const auto last = trick.end();
    const auto* winner = &iter->first;
    auto winner_card_type = dereference(iter->second.getType());

    for (++iter; iter != last; ++iter) {
        const auto card_type = dereference(iter->second.getType());
        if ((card_type.suit == trump && winner_card_type.suit != trump) ||
            (card_type.suit == winner_card_type.suit &&
             card_type.rank > winner_card_type.rank)) {
            winner = &iter->first;
            winner_card_type = card_type;
        }
    }

    return winner;
}

}
