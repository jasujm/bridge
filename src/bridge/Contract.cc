#include "bridge/Contract.hh"

#include "IoUtility.hh"

#include <map>
#include <ostream>

namespace Bridge {

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
    static const auto DOUBLING_NAMES = std::map<Doubling, const char*>{
        { Doubling::UNDOUBLED, "undoubled" },
        { Doubling::DOUBLED,   "doubled"   },
        { Doubling::REDOUBLED, "redoubled" },
    };
    return outputEnum(os, doubling, DOUBLING_NAMES);
}

std::ostream& operator<<(std::ostream& os, const Contract& contract)
{
    return os << contract.bid << " " << contract.doubling;
}

}
