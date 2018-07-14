#include "bridge/BasicHand.hh"

#include <cassert>

namespace Bridge {

BasicHand::CardEntry::CardEntry(const Card& card) :
    card {card},
    isPlayed {false}
{
}

void BasicHand::handleSubscribe(std::weak_ptr<CardRevealStateObserver> observer)
{
    notifier.subscribe(std::move(observer));
}

void BasicHand::handleRequestReveal(const IndexVector& ns)
{
    notifier.notifyAll(CardRevealState::REQUESTED, ns);
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
