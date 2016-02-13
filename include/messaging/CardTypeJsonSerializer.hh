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

/** \brief JSON converter for Rank
 *
 * \sa JsonSerializer.hh, \ref jsoncardtype
 */
template<>
struct JsonConverter<Rank>
{
    /** \brief Convert Rank to JSON
     */
    static nlohmann::json convertToJson(Rank rank);

    /** \brief Convert JSON to Rank
     */
    static Rank convertFromJson(const nlohmann::json& j);
};

/** \brief JSON converter for Suit
 *
 * \sa JsonSerializer.hh, \ref jsoncardtype
 */
template<>
struct JsonConverter<Suit>
{
    /** \brief Convert Suit to JSON
     */
    static nlohmann::json convertToJson(Suit suit);

    /** \brief Convert JSON to Suit
     */
    static Suit convertFromJson(const nlohmann::json& j);
};

/** \brief JSON converter for CardType
 *
 * \sa JsonSerializer.hh, \ref jsoncardtype
 */
template<>
struct JsonConverter<CardType>
{
    /** \brief Convert CardType to JSON
     */
    static nlohmann::json convertToJson(const CardType& cardType);

    /** \brief Convert JSON to CardType
     */
    static CardType convertFromJson(const nlohmann::json& j);
};

}
}

#endif // MESSAGING_CARDJSONSERIALIZER_HH_
