#include "bridge/CardType.hh"

#include "IoUtility.hh"

#include <istream>
#include <ostream>
#include <string>

namespace Bridge {

namespace {

using namespace std::string_literals;
using RankStringRelation = RankToStringMap::value_type;
using SuitStringRelation = SuitToStringMap::value_type;

const auto RANK_STRING_PAIRS = {
    RankStringRelation { Rank::TWO,   "2"s     },
    RankStringRelation { Rank::THREE, "3"s     },
    RankStringRelation { Rank::FOUR,  "4"s     },
    RankStringRelation { Rank::FIVE,  "5"s     },
    RankStringRelation { Rank::SIX,   "6"s     },
    RankStringRelation { Rank::SEVEN, "7"s     },
    RankStringRelation { Rank::EIGHT, "8"s     },
    RankStringRelation { Rank::NINE,  "9"s     },
    RankStringRelation { Rank::TEN,   "10"s    },
    RankStringRelation { Rank::JACK,  "jack"s  },
    RankStringRelation { Rank::QUEEN, "queen"s },
    RankStringRelation { Rank::KING,  "king"s  },
    RankStringRelation { Rank::ACE,   "ace"s   },
};

const auto SUIT_STRING_PAIRS = {
    SuitStringRelation { Suit::CLUBS,    "clubs"s    },
    SuitStringRelation { Suit::DIAMONDS, "diamonds"s },
    SuitStringRelation { Suit::HEARTS,   "hearts"s   },
    SuitStringRelation { Suit::SPADES,   "spades"s   },
};

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
