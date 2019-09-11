#include "bridge/BasicTrick.hh"

#include "Utility.hh"

#include <cassert>

namespace Bridge {

BasicTrick::~BasicTrick() = default;

void BasicTrick::handleAddCardToTrick(const Card& card)
{
    cards.emplace_back(card);
}

int BasicTrick::handleGetNumberOfCardsPlayed() const
{
    return cards.size();
}

const Card& BasicTrick::handleGetCard(const int n) const
{
    assert(0 <= n && n < ssize(cards));
    return cards[n];
}

const Hand& BasicTrick::handleGetHand(const int n) const
{
    assert(0 <= n && n < ssize(hands));
    return hands[n];
}

}
