#include "bridge/Contract.hh"
#include "bridge/Partnership.hh"
#include "bridge/Vulnerability.hh"
#include "scoring/DuplicateScoreSheet.hh"
#include "scoring/DuplicateScoring.hh"

#include <gtest/gtest.h>

#include <array>
#include <algorithm>

using Bridge::Scoring::DuplicateScoreSheet;
using Bridge::Scoring::calculateDuplicateScore;

namespace {
constexpr Bridge::Partnership PARTNERSHIP {Bridge::Partnership::NORTH_SOUTH};
constexpr Bridge::Contract CONTRACT {
    Bridge::Bid {1, Bridge::Strain::CLUBS},
    Bridge::Doubling::UNDOUBLED};
constexpr int TRICKS_WON = 7;
constexpr Bridge::Vulnerability VULNERABILITY {false, true};
}

class DuplicateScoreSheetTest : public testing::Test
{
protected:
    const std::array<boost::optional<DuplicateScoreSheet::Score>, 4> entries {{
        boost::none,
        boost::make_optional(
            DuplicateScoreSheet::Score {
                PARTNERSHIP,
                calculateDuplicateScore(
                    CONTRACT, false, TRICKS_WON).second}),
        boost::make_optional(
            DuplicateScoreSheet::Score {
                otherPartnership(PARTNERSHIP),
                calculateDuplicateScore(
                    CONTRACT, false, TRICKS_WON - 1).second}),
        boost::make_optional(
            DuplicateScoreSheet::Score {
                otherPartnership(PARTNERSHIP),
                calculateDuplicateScore(
                    CONTRACT, true, TRICKS_WON).second}),
    }};
};

TEST_F(DuplicateScoreSheetTest, testInitialEntries)
{
    DuplicateScoreSheet score_sheet(entries.begin(), entries.end());
    EXPECT_TRUE(
        std::equal(
            entries.begin(), entries.end(),
            score_sheet.begin(), score_sheet.end()));
}

TEST_F(DuplicateScoreSheetTest, testAddingEntries)
{
    DuplicateScoreSheet score_sheet;

    // Put different scores to scoresheet and test that they match

    score_sheet.addPassedOut();
    score_sheet.addResult(
        PARTNERSHIP, CONTRACT, TRICKS_WON, VULNERABILITY);
    score_sheet.addResult(
        PARTNERSHIP, CONTRACT, TRICKS_WON - 1, VULNERABILITY);
    score_sheet.addResult(
        otherPartnership(PARTNERSHIP), CONTRACT, TRICKS_WON, VULNERABILITY);

    EXPECT_TRUE(
        std::equal(
            entries.begin(), entries.end(),
            score_sheet.begin(), score_sheet.end()));
}
