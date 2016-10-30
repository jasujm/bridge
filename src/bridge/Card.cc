#include "bridge/Card.hh"

#include "bridge/CardType.hh"

#include <boost/optional/optional.hpp>

namespace Bridge {

Card::~Card() = default;

boost::optional<CardType> Card::getType() const
{
    if (!isKnown()) {
        return boost::none;
    }
    return handleGetType();
}

bool Card::isKnown() const
{
    return handleIsKnown();
}

}
