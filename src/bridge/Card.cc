#include "bridge/Card.hh"

#include "bridge/CardType.hh"

namespace Bridge {

Card::~Card() = default;

std::optional<CardType> Card::getType() const
{
    if (!isKnown()) {
        return std::nullopt;
    }
    return handleGetType();
}

bool Card::isKnown() const
{
    return handleIsKnown();
}

}
