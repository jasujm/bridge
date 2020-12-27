#include "bridge/Contract.hh"

#include <ostream>

namespace Bridge {

bool isMade(const Contract& contract, const int tricksWon)
{
    return tricksWon >= N_TRICKS_IN_BOOK + contract.bid.level;
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
