#include "bridge/BasicTrick.hh"

#include <cassert>

namespace Bridge {

BasicTrick::~BasicTrick() = default;

void BasicTrick::handleAddCardToTrick(const Card& card)
{
    cards.emplace_back(card);
}

std::size_t BasicTrick::handleGetNumberOfCardsPlayed() const
{
    return cards.size();
}

const Card& BasicTrick::handleGetCard(const std::size_t n) const
{
    assert(n < cards.size());
    return cards[n];
}

const Hand& BasicTrick::handleGetHand(const std::size_t n) const
{
    assert(n < hands.size());
    return hands[n];
}

}
