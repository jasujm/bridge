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
    return handleIsShuffleCompleted();
}

boost::optional<std::size_t> CardManager::getNumberOfCards() const
{
    if (isShuffleCompleted()) {
        return handleGetNumberOfCards();
    }
    return boost::none;
}

}
}
