#include "bridge/Bid.hh"

#include "IoUtility.hh"

#include <boost/optional/optional.hpp>

#include <initializer_list>
#include <istream>
#include <ostream>
#include <string>

namespace Bridge {

namespace {

const auto STRAIN_STRING_PAIRS =
    std::initializer_list<StrainToStringMap::value_type> {
    { Strain::CLUBS,    "clubs"    },
    { Strain::DIAMONDS, "diamonds" },
    { Strain::HEARTS,   "hearts"   },
    { Strain::SPADES,   "spades"   },
    { Strain::NO_TRUMP, "notrump"  }};

}

const StrainToStringMap STRAIN_TO_STRING_MAP(
    STRAIN_STRING_PAIRS.begin(), STRAIN_STRING_PAIRS.end());

const Bid Bid::LOWEST_BID {Bid::MINIMUM_LEVEL, Strain::CLUBS};
const Bid Bid::HIGHEST_BID {Bid::MAXIMUM_LEVEL, Strain::NO_TRUMP};

boost::optional<Bid> nextHigherBid(const Bid& bid)
{
    if (bid.strain < Strain::NO_TRUMP) {
        // Safe because strains are continuous with Strain::NO_TRUMP being the
        // highest
        return Bid {
            bid.level, static_cast<Strain>(static_cast<int>(bid.strain) + 1)};
    } else if (bid.level < Bid::MAXIMUM_LEVEL) {
        return Bid {bid.level + 1, Strain::CLUBS};
    }
    return boost::none;
}

bool operator==(const Bid& lhs, const Bid& rhs)
{
    return &lhs == &rhs || (lhs.level == rhs.level && lhs.strain == rhs.strain);
}

bool operator<(const Bid& lhs, const Bid& rhs)
{
    return (lhs.level < rhs.level ||
            static_cast<int>(lhs.strain) < static_cast<int>(rhs.strain));
}

std::ostream& operator<<(std::ostream& os, Strain strain)
{
    return outputEnum(os, strain, STRAIN_TO_STRING_MAP.left);
}

std::ostream& operator<<(std::ostream& os, const Bid& bid)
{
    return os << bid.level << " " << bid.strain;
}

std::istream& operator>>(std::istream& is, Strain& strain)
{
    return inputEnum(is, strain, STRAIN_TO_STRING_MAP.right);
}

}
