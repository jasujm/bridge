#include "messaging/DuplicateScoreSheetJsonSerializer.hh"

#include "scoring/DuplicateScoreSheet.hh"
#include "messaging/JsonSerializerUtility.hh"
#include "messaging/PartnershipJsonSerializer.hh"

#include <boost/iterator/transform_iterator.hpp>

#include <iostream>

using Bridge::Scoring::DuplicateScoreSheet;

using nlohmann::json;

namespace Bridge {
namespace Messaging {

const std::string DUPLICATE_SCORE_SHEET_PARTNERSHIP_KEY {"partnership"};
const std::string DUPLICATE_SCORE_SHEET_SCORE_KEY {"score"};

template<>
json toJson(const DuplicateScoreSheet::Score& score)
{
    return {
        {DUPLICATE_SCORE_SHEET_PARTNERSHIP_KEY, toJson(score.partnership)},
        {DUPLICATE_SCORE_SHEET_SCORE_KEY, toJson(score.score)}};
}

template<>
DuplicateScoreSheet::Score fromJson(const json& j)
{
    return {
        checkedGet<Partnership>(j, DUPLICATE_SCORE_SHEET_PARTNERSHIP_KEY),
        validate(
            checkedGet<int>(j, DUPLICATE_SCORE_SHEET_SCORE_KEY),
            [](const int score) { return score > 0; })};
}

template<>
json toJson(const DuplicateScoreSheet& scoreSheet)
{
    auto ret = json::array();
    for (const auto& entry : scoreSheet) {
        ret.push_back(optionalToJson(entry));
    }
    return ret;
}

template<>
DuplicateScoreSheet fromJson(const json& j)
{
    return DuplicateScoreSheet(
        boost::make_transform_iterator(
            j.begin(), jsonToOptional<DuplicateScoreSheet::Score>),
        boost::make_transform_iterator(
            j.end(),   jsonToOptional<DuplicateScoreSheet::Score>));
}

}
}
