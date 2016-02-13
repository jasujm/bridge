/** \file
 *
 * \brief Definition of JSON serializer for Bridge::DuplicateScoreSheet
 *
 * \page jsonduplicatescoresheet Duplicate score sheet JSON representation
 *
 * A Bridge::DuplicateScoreSheet is represented by a JSON array consisting of
 * entries, each corresponding to one passed‐out or played deal. For
 * passed‐out deals the corresponding entry is \c null. For played deals the
 * entry is a JSON object consisting of the following:
 *
 * \code{.json}
 * {
 *     { "partnership": <partnership> },
 *     { "score": <score> }
 * }
 * \endcode
 *
 * - &lt;partnership&gt; is a string representing partnership that scored. It
 *   must be one of the following: "northSouth", "eastWest".
 * - &lt;score&gt; is a positive integer representing the amount scored.
 */

#ifndef MESSAGING_DUPLICATESCORESHEETJSONSERIALIZER_HH_
#define MESSAGING_DUPLICATESCORESHEETJSONSERIALIZER_HH_

#include "scoring/DuplicateScoreSheet.hh"
#include "messaging/JsonSerializer.hh"

#include <string>

namespace Bridge {
namespace Messaging {

/** \brief Key for DuplicateScoreSheet::Score::partnerhip
 *
 * \sa \ref jsonduplicatescoresheet
 */
extern const std::string DUPLICATE_SCORE_SHEET_PARTNERSHIP_KEY;

/** \brief Key for DuplicateScoreSheet::Score::score
 *
 * \sa \ref jsonduplicatescoresheet
 */
extern const std::string DUPLICATE_SCORE_SHEET_SCORE_KEY;

/** \brief JSON converter for Scoring::DuplicateScoreSheet::Score
 *
 * \sa JsonSerializer.hh, \ref jsonduplicatescoresheet
 */
template<>
struct JsonConverter<Scoring::DuplicateScoreSheet::Score>
{
    /** \brief Convert Scoring::DuplicateScoreSheet::Score to JSON
     */
    static nlohmann::json convertToJson(
        const Scoring::DuplicateScoreSheet::Score& score);

    /** \brief Convert JSON to Scoring::DuplicateScoreSheet::Score
     */
    static Scoring::DuplicateScoreSheet::Score convertFromJson(
        const nlohmann::json& j);
};

/** \brief JSON converter for Scoring::DuplicateScoreSheet
 *
 * \sa JsonSerializer.hh, \ref jsonduplicatescoresheet
 */
template<>
struct JsonConverter<Scoring::DuplicateScoreSheet>
{
    /** \brief Convert Scoring::DuplicateScoreSheet to JSON
     */
    static nlohmann::json convertToJson(
        const Scoring::DuplicateScoreSheet& scoreSheet);

    /** \brief Convert JSON to Scoring::DuplicateScoreSheet
     */
    static Scoring::DuplicateScoreSheet convertFromJson(
        const nlohmann::json& j);
};

}
}

#endif // MESSAGING_DUPLICATESCORESHEETJSONSERIALIZER_HH_
