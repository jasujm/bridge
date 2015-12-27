#ifndef MOCKHAND_HH_
#define MOCKHAND_HH_

#include "bridge/Card.hh"
#include "bridge/Hand.hh"

#include <boost/logic/tribool.hpp>
#include <gmock/gmock.h>

namespace Bridge {

class MockHand : public Hand {
public:
    MOCK_METHOD1(handleMarkPlayed, void(std::size_t));
    MOCK_CONST_METHOD1(handleGetCard, const Card&(std::size_t));
    MOCK_CONST_METHOD1(handleIsPlayed, bool(std::size_t));
    MOCK_CONST_METHOD0(handleGetNumberOfCards, std::size_t());
    MOCK_CONST_METHOD1(handleIsOutOfSuit, boost::logic::tribool(Suit));
};

}

#endif // MOCKHAND_HH_
