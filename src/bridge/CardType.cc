#include "bridge/CardType.hh"

#include <ostream>

namespace Bridge {

std::ostream& operator<<(std::ostream& os, const Rank rank)
{
    return os << rank.value();
}

std::ostream& operator<<(std::ostream& os, const Suit suit)
{
    return os << suit.value();
}

std::ostream& operator<<(std::ostream& os, const CardType cardType)
{
    return os << cardType.rank << " " << cardType.suit;
}

}
