#include "messaging/DuplicateScoreSheetJsonSerializer.hh"

#include "messaging/JsonSerializerUtility.hh"
#include "messaging/PartnershipJsonSerializer.hh"

#include <boost/iterator/transform_iterator.hpp>

using Bridge::Scoring::DuplicateScoreSheet;

using nlohmann::json;

namespace Bridge {
namespace Messaging {

const std::string DUPLICATE_SCORE_SHEET_PARTNERSHIP_KEY {"partnership"};
const std::string DUPLICATE_SCORE_SHEET_SCORE_KEY {"score"};

json JsonConverter<DuplicateScoreSheet::Score>::convertToJson(
    const DuplicateScoreSheet::Score& score)
{
    return {
        {DUPLICATE_SCORE_SHEET_PARTNERSHIP_KEY, toJson(score.partnership)},
        {DUPLICATE_SCORE_SHEET_SCORE_KEY, toJson(score.score)}};
}

DuplicateScoreSheet::Score
JsonConverter<DuplicateScoreSheet::Score>::convertFromJson(const json& j)
{
    return {
        checkedGet<Partnership>(j, DUPLICATE_SCORE_SHEET_PARTNERSHIP_KEY),
        validate(
            checkedGet<int>(j, DUPLICATE_SCORE_SHEET_SCORE_KEY),
            [](const int score) { return score > 0; })};
}

json JsonConverter<DuplicateScoreSheet>::convertToJson(
    const DuplicateScoreSheet& scoreSheet)
{
    auto ret = json::array();
    for (const auto& entry : scoreSheet) {
        ret.push_back(toJson(entry));
    }
    return ret;
}

DuplicateScoreSheet JsonConverter<DuplicateScoreSheet>::convertFromJson(
    const json& j)
{
    return DuplicateScoreSheet(
        boost::make_transform_iterator(
            j.begin(), fromJson<boost::optional<DuplicateScoreSheet::Score>>),
        boost::make_transform_iterator(
            j.end(),   fromJson<boost::optional<DuplicateScoreSheet::Score>>));
}

}
}
