#include "bridge/BasicTrick.hh"

#include "bridge/Card.hh"
#include "bridge/CardType.hh"
#include "bridge/Hand.hh"
#include "Utility.hh"

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

const Hand& BasicTrick::handleGetWinner() const
{
    assert(cards.size() == N_CARDS_IN_TRICK);

    auto winner_i = 0u;
    CardType winner_card_type {*cards.front().get().getType()};

    for (const auto i : from_to(1u, N_CARDS_IN_TRICK)) {
        const auto card_type = *cards[i].get().getType();
        if ((card_type.suit == trump && winner_card_type.suit != trump) ||
            (card_type.suit == winner_card_type.suit &&
             card_type.rank > winner_card_type.rank)) {
            winner_i = i;
            winner_card_type = card_type;
        }
    }

    return hands[winner_i];
}

}
