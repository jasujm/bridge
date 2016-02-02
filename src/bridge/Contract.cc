#include "bridge/Contract.hh"

#include "IoUtility.hh"

#include <ostream>

namespace Bridge {

namespace {

const auto DOUBLING_STRING_PAIRS =
    std::initializer_list<DoublingToStringMap::value_type> {
    { Doubling::UNDOUBLED, "undoubled" },
    { Doubling::DOUBLED,   "doubled"   },
    { Doubling::REDOUBLED, "redoubled" }};

}

const DoublingToStringMap DOUBLING_TO_STRING_MAP(
    DOUBLING_STRING_PAIRS.begin(), DOUBLING_STRING_PAIRS.end());

bool isMade(const Contract& contract, const int tricksWon)
{
    return tricksWon >= N_TRICKS_IN_BOOK + contract.bid.level;
}

bool operator==(const Contract& lhs, const Contract& rhs)
{
    return &lhs == &rhs ||
        (lhs.bid == rhs.bid &&
         lhs.doubling == rhs.doubling);
}

std::ostream& operator<<(std::ostream& os, const Doubling doubling)
{
    return outputEnum(os, doubling, DOUBLING_TO_STRING_MAP.left);
}

std::ostream& operator<<(std::ostream& os, const Contract& contract)
{
    return os << contract.bid << " " << contract.doubling;
}

}
