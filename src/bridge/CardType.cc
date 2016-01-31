#include "bridge/CardType.hh"

#include "IoUtility.hh"

#include <initializer_list>
#include <istream>
#include <ostream>
#include <string>

namespace Bridge {

namespace {

const auto RANK_STRING_PAIRS =
    std::initializer_list<RankToStringMap::value_type> {
    { Rank::TWO,   "2"     },
    { Rank::THREE, "3"     },
    { Rank::FOUR,  "4"     },
    { Rank::FIVE,  "5"     },
    { Rank::SIX,   "6"     },
    { Rank::SEVEN, "7"     },
    { Rank::EIGHT, "8"     },
    { Rank::NINE,  "9"     },
    { Rank::TEN,   "10"    },
    { Rank::JACK,  "jack"  },
    { Rank::QUEEN, "queen" },
    { Rank::KING,  "king"  },
    { Rank::ACE,   "ace"   }};

const auto SUIT_STRING_PAIRS =
    std::initializer_list<SuitToStringMap::value_type> {
    { Suit::CLUBS,    "clubs"    },
    { Suit::DIAMONDS, "diamonds" },
    { Suit::HEARTS,   "hearts"   },
    { Suit::SPADES,   "spades"   }};

}

const RankToStringMap RANK_TO_STRING_MAP(
    RANK_STRING_PAIRS.begin(), RANK_STRING_PAIRS.end());

const SuitToStringMap SUIT_TO_STRING_MAP(
    SUIT_STRING_PAIRS.begin(), SUIT_STRING_PAIRS.end());

bool operator==(const CardType& lhs, const CardType& rhs)
{
    return &lhs == &rhs ||
        (lhs.rank == rhs.rank && lhs.suit == rhs.suit);
}

std::ostream& operator<<(std::ostream& os, const Rank rank)
{
    return outputEnum(os, rank, RANK_TO_STRING_MAP.left);
}

std::ostream& operator<<(std::ostream& os, const Suit suit)
{
    return outputEnum(os, suit, SUIT_TO_STRING_MAP.left);
}

std::ostream& operator<<(std::ostream& os, const CardType cardType)
{
    return os << cardType.rank << " " << cardType.suit;
}

std::istream& operator>>(std::istream& is, Rank& rank)
{
    return inputEnum(is, rank, RANK_TO_STRING_MAP.right);
}

std::istream& operator>>(std::istream& is, Suit& suit)
{
    return inputEnum(is, suit, SUIT_TO_STRING_MAP.right);
}

}
