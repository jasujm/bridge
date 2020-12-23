#include "bridge/CardType.hh"

#include <ostream>

namespace Bridge {

bool operator==(const CardType& lhs, const CardType& rhs)
{
    return lhs.rank == rhs.rank && lhs.suit == rhs.suit;
}

bool operator<(const CardType& lhs, const CardType& rhs)
{
    return lhs.suit < rhs.suit || lhs.rank < rhs.rank;
}

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
