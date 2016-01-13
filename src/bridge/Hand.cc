#include "bridge/Hand.hh"

#include "bridge/Card.hh"
#include "bridge/CardType.hh"
#include "Utility.hh"

#include <algorithm>
#include <boost/logic/tribool.hpp>

namespace Bridge {

Hand::~Hand() = default;

void Hand::markPlayed(const std::size_t n)
{
    const auto n_cards = handleGetNumberOfCards();
    handleMarkPlayed(checkIndex(n, n_cards));
}

const Card* Hand::getCard(const std::size_t n) const
{
    if (isPlayed(n)) {
        return nullptr;
    }
    return &handleGetCard(n);
}

bool Hand::isPlayed(const std::size_t n) const
{
    const auto n_cards = handleGetNumberOfCards();
    return handleIsPlayed(checkIndex(n, n_cards));
}

std::size_t Hand::getNumberOfCards() const
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

boost::optional<std::size_t> findFromHand(
    const Hand& hand, const CardType& cardType)
{
    const auto n_cards = hand.getNumberOfCards();
    for (const auto n : to(n_cards)) {
        const auto card = hand.getCard(n);
        if (card && card->getType() == cardType) {
            return n;
        }
    }
    return boost::none;
}

}
