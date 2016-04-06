#include "engine/SimpleCardManager.hh"

#include "bridge/BasicHand.hh"
#include "bridge/BridgeConstants.hh"
#include "bridge/CardTypeIterator.hh"
#include "Utility.hh"

#include <algorithm>
#include <random>

namespace Bridge {
namespace Engine {

void SimpleCardManager::handleRequestShuffle()
{
    std::random_device rd;
    std::default_random_engine re {rd()};

    auto new_cards = std::vector<SimpleCard>(
        cardTypeIterator(0),
        cardTypeIterator(N_CARDS));
    std::shuffle(new_cards.begin(), new_cards.end(), re);

    cards.swap(new_cards);
    notifyAll(Shuffled {});
}

std::unique_ptr<Hand> SimpleCardManager::handleGetHand(const IndexRange ns)
{
    return std::make_unique<BasicHand>(
        containerAccessIterator(ns.begin(), cards),
        containerAccessIterator(ns.end(), cards));
}

std::size_t SimpleCardManager::handleGetNumberOfCards() const
{
    return cards.size();
}

bool SimpleCardManager::handleIsShuffleCompleted() const
{
    return !cards.empty();
}

}
}
