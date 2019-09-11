#include "engine/CardManager.hh"

#include <utility>

namespace Bridge {
namespace Engine {

CardManager::~CardManager() = default;

void CardManager::subscribe(std::weak_ptr<Observer<ShufflingState>> observer)
{
    handleSubscribe(std::move(observer));
}

void CardManager::requestShuffle()
{
    handleRequestShuffle();
}

bool CardManager::isShuffleCompleted() const
{
    return handleIsShuffleCompleted();
}

std::optional<int> CardManager::getNumberOfCards() const
{
    if (isShuffleCompleted()) {
        return handleGetNumberOfCards();
    }
    return std::nullopt;
}

}
}
