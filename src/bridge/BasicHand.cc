#include "bridge/BasicHand.hh"

#include "Utility.hh"

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

void BasicHand::handleMarkPlayed(const int n)
{
    assert(0 <= n && n < ssize(cards));
    cards[n].isPlayed = true;
}

const Card& BasicHand::handleGetCard(const int n) const
{
    assert(0 <= n && n < ssize(cards));
    return cards[n].card;
}

bool BasicHand::handleIsPlayed(const int n) const
{
    assert(0 <= n && n < ssize(cards));
    return cards[n].isPlayed;
}

int BasicHand::handleGetNumberOfCards() const
{
    return cards.size();
}

}
