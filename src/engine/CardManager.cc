#include "engine/CardManager.hh"

namespace Bridge {
namespace Engine {

CardManager::~CardManager() = default;

void CardManager::requestShuffle()
{
    handleRequestShuffle();
}

bool CardManager::isShuffleCompleted() const
{
    return getNumberOfCards() > 0;
}

std::size_t CardManager::getNumberOfCards() const
{
    return handleGetNumberOfCards();
}

}
}
