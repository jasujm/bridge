#include "bridge/Call.hh"

#include <ostream>

namespace Bridge {

std::ostream& operator<<(std::ostream& os, Pass)
{
    return os << "Pass";
}

std::ostream& operator<<(std::ostream& os, Double)
{
    return os << "Double";
}

std::ostream& operator<<(std::ostream& os, Redouble)
{
    return os << "Redouble";
}

}
