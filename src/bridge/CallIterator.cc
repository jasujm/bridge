#include "bridge/CallIterator.hh"

#include "bridge/BridgeConstants.hh"

#include <stdexcept>

namespace Bridge {

namespace {

constexpr auto PASS_INDEX = N_BIDS;
constexpr auto DOUBLE_INDEX = N_BIDS + 1;
constexpr auto REDOUBLE_INDEX = N_BIDS + 2;

struct CallIndexVisitor {
    int operator()(const Bid& bid)
    {
        const auto n_strain = static_cast<int>(bid.strain.get());
        return (bid.level - 1) * Strain::size() + n_strain;
    }

    int operator()(Pass)
    {
        return PASS_INDEX;
    }

    int operator()(Double)
    {
        return DOUBLE_INDEX;
    }

    int operator()(Redouble)
    {
        return REDOUBLE_INDEX;
    }
};

}

int callIndex(const Call& call)
{
    return std::visit(CallIndexVisitor {}, call);
}

Call enumerateCall(const int n)
{
    if (0 <= n && n < N_BIDS) {
        constexpr auto N_STRAINS = Strain::size();
        const auto level = n / N_STRAINS;
        const auto n_strain = n % N_STRAINS;
        return Bid {
            static_cast<int>(level) + 1,
            static_cast<StrainLabel>(n_strain)};
    } else {
        switch (n)
        {
        case PASS_INDEX:
            return Pass {};
        case DOUBLE_INDEX:
            return Double {};
        case REDOUBLE_INDEX:
            return Redouble {};
        default:
            throw std::invalid_argument {"Invalid call number"};
        }
    }
}

}
