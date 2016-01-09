#ifndef MOCKCARDMANAGER_HH_
#define MOCKCARDMANAGER_HH_

#include "engine/CardManager.hh"

#include <gmock/gmock.h>

namespace Bridge {
namespace Engine {

class MockCardManager : public CardManager {
public:
    using Observable<Shuffled>::notifyAll;
    MOCK_METHOD0(handleRequestShuffle, void());
    MOCK_METHOD1(handleGetHand,
                 std::unique_ptr<Hand>(const std::vector<std::size_t>&));
    MOCK_CONST_METHOD0(handleGetNumberOfCards, std::size_t());
};

}
}

#endif // MOCKCARDMANAGER_HH_
