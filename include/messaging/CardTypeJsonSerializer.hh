/** \file
 *
 * \brief Definition of JSON serializer for Bridge::CardType
 *
 * \page jsoncardtype Card type JSON representation
 *
 * A Bridge::CardType is represented by a JSON object consisting of the
 * following:
 *
 * \code{.json}
 * {
 *     { "rank": <rank> },
 *     { "suit": <suit> }
 * }
 * \endcode
 *
 * - &lt;rank&gt; is a string representing the rank of the card. It must be
 *   one of the following: "2", "3" "4", "5", "6", "7", "8", "9", "10",
 *   "jack", "queen", "king", "ace".
 * - &lt;suit&gt; is a string representing the strain of the bid. It must be
 *   one of the following: "clubs", "diamonds", "hearts", "spades".
 */

#ifndef MESSAGING_CARDJSONSERIALIZER_HH_
#define MESSAGING_CARDJSONSERIALIZER_HH_

#include <nlohmann/json.hpp>

#include <string>

namespace Bridge {

class CardType;

/** \brief Key for CardType::rank
 *
 * \sa \ref jsoncardtype
 */
extern const std::string CARD_TYPE_RANK_KEY;

/** \brief Key for CardType::suit
 *
 * \sa \ref jsoncardtype
 */
extern const std::string CARD_TYPE_SUIT_KEY;

/** \brief Convert CardType to JSON
 */
void to_json(nlohmann::json&, const CardType&);

/** \brief Convert JSON to CardType
 */
void from_json(const nlohmann::json& j, CardType&);

}

#endif // MESSAGING_CARDJSONSERIALIZER_HH_
