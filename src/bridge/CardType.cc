#include "bridge/CardType.hh"

#include "IoUtility.hh"

#include <boost/bimap/bimap.hpp>

#include <initializer_list>
#include <istream>
#include <ostream>
#include <string>

namespace Bridge {

namespace {

using RankMap = boost::bimaps::bimap<Rank, std::string>;
const auto RANK_STR_PAIRS = std::initializer_list<RankMap::value_type>{
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
    { Rank::ACE,   "ace"   },
};
const RankMap RANK_MAPPING {RANK_STR_PAIRS.begin(), RANK_STR_PAIRS.end()};

using SuitMap = boost::bimaps::bimap<Suit, std::string>;
const auto SUIT_STR_PAIRS = std::initializer_list<SuitMap::value_type>{
    { Suit::CLUBS,    "clubs"    },
    { Suit::DIAMONDS, "diamonds" },
    { Suit::HEARTS,   "hearts"   },
    { Suit::SPADES,   "spades"   },
};
const SuitMap SUIT_MAPPING {SUIT_STR_PAIRS.begin(), SUIT_STR_PAIRS.end()};

}

bool operator==(const CardType& lhs, const CardType& rhs)
{
    return &lhs == &rhs ||
        (lhs.rank == rhs.rank && lhs.suit == rhs.suit);
}

std::ostream& operator<<(std::ostream& os, const Rank rank)
{
    return outputEnum(os, rank, RANK_MAPPING.left);
}

std::ostream& operator<<(std::ostream& os, const Suit suit)
{
    return outputEnum(os, suit, SUIT_MAPPING.left);
}

std::ostream& operator<<(std::ostream& os, const CardType cardType)
{
    return os << cardType.rank << " " << cardType.suit;
}

std::istream& operator>>(std::istream& is, Rank& rank)
{
    return inputEnum(is, rank, RANK_MAPPING.right);
}

std::istream& operator>>(std::istream& is, Suit& suit)
{
    return inputEnum(is, suit, SUIT_MAPPING.right);
}

}
