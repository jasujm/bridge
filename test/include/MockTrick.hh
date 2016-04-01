#ifndef MOCKTRICK_HH_
#define MOCKTRICK_HH_

#include "bridge/Card.hh"
#include "bridge/Hand.hh"
#include "bridge/Trick.hh"

#include <gmock/gmock.h>

namespace Bridge {

class MockTrick : public Trick {
public:
    MOCK_METHOD1(handleAddCardToTrick, void(const Card&));
    MOCK_CONST_METHOD0(handleGetNumberOfCardsPlayed, std::size_t());
    MOCK_CONST_METHOD1(handleGetCard, const Card&(std::size_t));
    MOCK_CONST_METHOD1(handleGetHand, const Hand&(std::size_t));
};

}

#endif // MOCKTRICK_HH_
