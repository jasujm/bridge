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

#include "messaging/JsonSerializer.hh"

#include <string>

namespace Bridge {

class CardType;
enum class Rank;
enum class Suit;

namespace Messaging {

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

/** \sa \ref jsoncardtype
 */
template<>
nlohmann::json toJson(const Rank& rank);

/** \sa \ref jsoncardtype
 */
template<>
Rank fromJson(const nlohmann::json& j);

/** \sa \ref jsoncardtype
 */
template<>
nlohmann::json toJson(const Suit& suit);

/** \sa \ref jsoncardtype
 */
template<>
Suit fromJson(const nlohmann::json& j);

/** \sa \ref jsoncardtype
 */
template<>
nlohmann::json toJson(const CardType& cardType);

/** \sa \ref jsoncardtype
 */
template<>
CardType fromJson(const nlohmann::json& j);

}
}

#endif // MESSAGING_CARDJSONSERIALIZER_HH_
