#include "bridge/BasicHand.hh"

#include <cassert>

namespace Bridge {

BasicHand::CardEntry::CardEntry(const Card& card) :
    card {card},
    isPlayed {false}
{
}

void BasicHand::handleMarkPlayed(const std::size_t n)
{
    assert(n < cards.size());
    cards[n].isPlayed = true;
}

const Card& BasicHand::handleGetCard(const std::size_t n) const
{
    assert(n < cards.size());
    return cards[n].card;
}

bool BasicHand::handleIsPlayed(const std::size_t n) const
{
    assert(n < cards.size());
    return cards[n].isPlayed;
}

std::size_t BasicHand::handleGetNumberOfCards() const
{
    return cards.size();
}

}
