#include "bridge/BasicTrick.hh"

#include "bridge/Card.hh"
#include "bridge/Hand.hh"

#include <boost/logic/tribool.hpp>

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

bool BasicTrick::handleIsPlayAllowed(const Hand& hand, const Card& card) const
{
    if (cards.empty()) {
        return true;
    }

    const auto suit = card.getType()->suit;
    const auto first_suit = cards.front().get().getType()->suit;
    return suit == first_suit || hand.isOutOfSuit(first_suit);
}

}
