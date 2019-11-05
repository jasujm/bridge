#include "bridge/RevealableCard.hh"

#include <cassert>

namespace Bridge {

void RevealableCard::reveal(const CardType& cardType)
{
    this->cardType = cardType;
}

CardType RevealableCard::handleGetType() const
{
    assert(cardType);
    return *cardType;
}

bool RevealableCard::handleIsKnown() const
{
    return bool(cardType);
}

}
