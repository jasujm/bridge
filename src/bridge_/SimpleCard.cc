#include "bridge/SimpleCard.hh"

namespace Bridge {

SimpleCard::SimpleCard(const CardType& cardType) :
    cardType {cardType}
{
}

CardType SimpleCard::handleGetType() const
{
    return cardType;
}

bool SimpleCard::handleIsKnown() const
{
    return true;
}

}
