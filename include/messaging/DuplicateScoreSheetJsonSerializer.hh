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

#include "messaging/JsonSerializer.hh"

#include <string>

namespace Bridge {

namespace Scoring {
class DuplicateScoreSheet;
}

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

/** \sa \ref jsonduplicatescoresheet
 */
template<>
nlohmann::json toJson(const Scoring::DuplicateScoreSheet& scoreSheet);

/** \sa \ref jsonduplicatescoresheet
 */
template<>
Scoring::DuplicateScoreSheet fromJson(const nlohmann::json& j);

}
}

#endif // MESSAGING_DUPLICATESCORESHEETJSONSERIALIZER_HH_
