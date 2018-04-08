#include "bridge/BridgeConstants.hh"
#include "bridge/CardShuffle.hh"
#include "bridge/CardTypeIterator.hh"
#include "bridge/Random.hh"

#include <algorithm>

namespace Bridge {

std::vector<CardType> generateShuffledDeck()
{
    auto cards = std::vector<CardType>(
        cardTypeIterator(0), cardTypeIterator(N_CARDS));
    std::shuffle(cards.begin(), cards.end(), getRng());
    return cards;
}

}
