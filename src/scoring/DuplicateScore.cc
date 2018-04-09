#include "scoring/DuplicateScore.hh"

namespace Bridge {
namespace Scoring {

bool operator==(const DuplicateScore& lhs, const DuplicateScore& rhs)
{
    return lhs.partnership == rhs.partnership && lhs.score == rhs.score;
}

std::ostream& operator<<(std::ostream& os, const DuplicateScore& score)
{
    return os << score.partnership << " " << score.score;
}

}
}
