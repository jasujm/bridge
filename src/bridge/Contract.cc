#include "bridge/Contract.hh"

#include <ostream>

namespace Bridge {

bool isMade(const Contract& contract, const int tricksWon)
{
    return tricksWon >= N_TRICKS_IN_BOOK + contract.bid.level;
}

bool operator==(const Contract& lhs, const Contract& rhs)
{
    return std::pair {lhs.bid, lhs.doubling} ==
        std::pair {rhs.bid, rhs.doubling};
}

std::ostream& operator<<(std::ostream& os, const Doubling doubling)
{
    return os << doubling.value();
}

std::ostream& operator<<(std::ostream& os, const Contract& contract)
{
    return os << contract.bid << " " << contract.doubling;
}

}
