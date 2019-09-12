#include "bridge/Hand.hh"

#include "bridge/Card.hh"
#include "bridge/CardType.hh"
#include "Utility.hh"

#include <boost/logic/tribool.hpp>

#include <algorithm>
#include <utility>

namespace Bridge {

Hand::~Hand() = default;

void Hand::subscribe(std::weak_ptr<CardRevealStateObserver> observer)
{
    handleSubscribe(std::move(observer));
}

void Hand::markPlayed(const int n)
{
    const auto n_cards = handleGetNumberOfCards();
    handleMarkPlayed(checkIndex(n, n_cards));
}

const Card* Hand::getCard(const int n) const
{
    if (isPlayed(n)) {
        return nullptr;
    }
    return &handleGetCard(n);
}

bool Hand::isPlayed(const int n) const
{
    const auto n_cards = handleGetNumberOfCards();
    return handleIsPlayed(checkIndex(n, n_cards));
}

int Hand::getNumberOfCards() const
{
    return handleGetNumberOfCards();
}

boost::logic::tribool Hand::isOutOfSuit(const Suit suit) const
{
    const auto out_of_suit = handleIsOutOfSuit(suit);
    if (!boost::indeterminate(out_of_suit))  {
        return out_of_suit;
    }

    // If card with matching suit is found, we know for sure that the hand is
    // not out of suit. However, if no card with maching suit is found, the
    // result may be indeterminate unless types of all cards are known.

    auto ret_if_given_suit_not_found = boost::logic::tribool {true};
    for (const auto& card : *this) {
            const auto type = card.getType();
            if (!type) {
                ret_if_given_suit_not_found = boost::indeterminate;
            } else if (type->suit == suit) {
                return false;
            }
    }
    return ret_if_given_suit_not_found;
}

boost::logic::tribool Hand::handleIsOutOfSuit(const Suit) const
{
    return boost::indeterminate;
}

std::optional<int> findFromHand(
    const Hand& hand, const CardType& cardType)
{
    const auto n_cards = hand.getNumberOfCards();
    for (const auto n : to(n_cards)) {
        const auto card = hand.getCard(n);
        if (card && card->getType() == cardType) {
            return n;
        }
    }
    return std::nullopt;
}

bool canBePlayedFromHand(const Hand& hand, int n)
{
    return 0 <= n && n < hand.getNumberOfCards() && !hand.isPlayed(n);
}

void requestRevealHand(Hand& hand)
{
    const auto range = to(hand.getNumberOfCards());
    hand.requestReveal(range.begin(), range.end());
}

}
