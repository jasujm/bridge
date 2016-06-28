#include "bridge/HandBase.hh"

#include <cassert>
#include <utility>

namespace Bridge {

HandBase::CardEntry::CardEntry(const Card& card) :
    card {card},
    isPlayed {false}
{
}

void HandBase::handleSubscribe(std::weak_ptr<CardRevealStateObserver> observer)
{
    CardRevealStateObserver::ObservableType::subscribe(std::move(observer));
}

void HandBase::handleMarkPlayed(const std::size_t n)
{
    assert(n < cards.size());
    cards[n].isPlayed = true;
}

const Card& HandBase::handleGetCard(const std::size_t n) const
{
    assert(n < cards.size());
    return cards[n].card;
}

bool HandBase::handleIsPlayed(const std::size_t n) const
{
    assert(n < cards.size());
    return cards[n].isPlayed;
}

std::size_t HandBase::handleGetNumberOfCards() const
{
    return cards.size();
}

}
