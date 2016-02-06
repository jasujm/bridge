#include "scoring/DuplicateScoreSheet.hh"

#include "bridge/Contract.hh"
#include "bridge/Vulnerability.hh"
#include "scoring/DuplicateScoring.hh"

#include <boost/optional/optional_io.hpp>

#include <algorithm>
#include <iterator>

namespace Bridge {
namespace Scoring {

DuplicateScoreSheet::DuplicateScoreSheet() = default;

DuplicateScoreSheet::DuplicateScoreSheet(std::initializer_list<Entry> entries) :
    entries {entries}
{
}

void DuplicateScoreSheet::addResult(
    Partnership partnership, const Contract& contract, const int tricksWon,
    const Vulnerability& vulnerability)
{
    const auto score = calculateDuplicateScore(
        contract, isVulnerable(vulnerability, partnership), tricksWon);
    if (!score.first) {
        partnership = otherPartnership(partnership);
    }
    entries.emplace_back(Score {partnership, score.second});
}

void DuplicateScoreSheet::addPassedOut()
{
    entries.emplace_back(boost::none);
}

bool operator==(
    const DuplicateScoreSheet& lhs, const DuplicateScoreSheet& rhs)
{
    return &lhs == &rhs ||
        std::equal(lhs.begin(), lhs.end(), rhs.begin(), rhs.end());
}

bool operator==(
    const DuplicateScoreSheet::Score& lhs,
    const DuplicateScoreSheet::Score& rhs)
{
    return lhs.partnership == rhs.partnership && lhs.score == rhs.score;
}

std::ostream& operator<<(
    std::ostream& os, const DuplicateScoreSheet& scoreSheet)
{
    std::copy(
        scoreSheet.begin(), scoreSheet.end(),
        std::ostream_iterator<DuplicateScoreSheet::Entry>(os, "\n"));
    return os;
}

std::ostream& operator<<(
    std::ostream& os, const DuplicateScoreSheet::Score& score)
{
    return os << score.partnership << ": " << score.score;
}

}
}
