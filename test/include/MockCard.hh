#ifndef MOCKCARD_HH_
#define MOCKCARD_HH_

#include "bridge/Card.hh"
#include "bridge/CardType.hh"

#include <gmock/gmock.h>

namespace Bridge {

class MockCard : public Card {
public:
    MOCK_CONST_METHOD0(handleGetType, CardType());
    MOCK_CONST_METHOD0(handleIsKnown, bool());
};

}

#endif // MOCKCARD_HH_
