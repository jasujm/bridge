#include "bridge/Bid.hh"

#include <ostream>
#include <tuple>

namespace Bridge {

const Bid Bid::LOWEST_BID {Bid::MINIMUM_LEVEL, Strains::CLUBS};
const Bid Bid::HIGHEST_BID {Bid::MAXIMUM_LEVEL, Strains::NO_TRUMP};

std::optional<Bid> nextHigherBid(const Bid& bid)
{
    if (bid.strain < Strains::NO_TRUMP) {
        // Safe because strains are continuous with Strain::NO_TRUMP being the
        // highest
        return Bid {
            bid.level,
            static_cast<StrainLabel>(static_cast<int>(bid.strain.get()) + 1)};
    } else if (bid.level < Bid::MAXIMUM_LEVEL) {
        return Bid {bid.level + 1, Strains::CLUBS};
    }
    return std::nullopt;
}

bool operator==(const Bid& lhs, const Bid& rhs)
{
    return std::tie(lhs.level, lhs.strain) ==
        std::tie(rhs.level, lhs.strain);
}

bool operator<(const Bid& lhs, const Bid& rhs)
{
    return std::tie(lhs.level, lhs.strain) <
        std::tie(rhs.level, rhs.strain);
}

std::ostream& operator<<(std::ostream& os, Strain strain)
{
    return os << strain.value();
}

std::ostream& operator<<(std::ostream& os, const Bid& bid)
{
    return os << bid.level << " " << bid.strain;
}

}
