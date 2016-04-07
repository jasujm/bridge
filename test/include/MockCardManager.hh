#ifndef MOCKCARDMANAGER_HH_
#define MOCKCARDMANAGER_HH_

#include "engine/CardManager.hh"

#include <boost/optional/optional.hpp>

#include <gmock/gmock.h>

namespace Bridge {
namespace Engine {

class MockCardManager : public CardManager {
public:
    MOCK_METHOD1(handleSubscribe, void(std::weak_ptr<Observer<Shuffled>>));
    MOCK_METHOD0(handleRequestShuffle, void());
    MOCK_METHOD1(handleGetHand, std::unique_ptr<Hand>(IndexRange));
    MOCK_CONST_METHOD0(handleIsShuffleCompleted, bool());
    MOCK_CONST_METHOD0(handleGetNumberOfCards, std::size_t());
};

}
}

#endif // MOCKCARDMANAGER_HH_
