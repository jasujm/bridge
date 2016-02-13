/** \file
 *
 * \brief Definition of JSON serializer for Bridge::DealState
 *
 * \page jsondealstate Deal state JSON representation
 *
 * A Bridge::DealState is represented by a JSON object consisting of the
 * following:
 *
 * \code{.json}
 * {
 *     "stage": <stage>,
 *     "positionInTurn": <positionInTurn>,
 *     "vulnerability": <vulnerability>,
 *     "cards": <cards>,
 *     "calls": <calls>,
 *     "declarer": <declarer>,
 *     "contract": <contract>,
 *     "currentTrick": <currentTrick>,
 *     "tricksWon": <tricksWon>
 * }
 * \endcode
 *
 * - &lt;stage&gt; is a string representing the stage of the deal. It must be
 *   one of the following: "shuffling", "bidding", "playing", "ended".
 * - &lt;positionInTurn&gt; is the position that has turn. It must be one of
 *   the following: "north", "east", "south", "west". Optional.
 * - &lt;vulnerability&gt; is the vulnerability status of the current
 *   deal. See \ref jsonvulnerability. Optional.
 * - &lt;cards&gt; See \ref jsondealstatecards. Optional.
 * - &lt;calls&gt; See \ref jsondealstatecalls. Optional.
 * - &lt;declarer&gt; is the position of the declarer. It has same values as
 *   &lt;positionInTurn&gt;. Optional.
 * - &lt;contract&gt; is the contract reached in the bidding stage. See \ref
 *   jsoncontract. Optional.
 * - &lt;currentTrick&gt; is the current trick in playing stage. See \ref
 *   jsondealstatetrick. Optional.
 * - &lt;tricksWon&gt; is the number of tricks won by each partnership in
 *   playing stage. See \ref jsontrickswon. Optional
 *
 * \section jsondealstatecards Cards JSON representation
 *
 * Cards held by the players are represented by a JSON object consisting of
 * the following:
 *
 * \code{.json}
 * {
 *     { "north": <cards> },
 *     { "east": <cards> },
 *     { "south": <cards> },
 *     { "west": <cards> }
 * }
 * \endcode
 *
 * For each position, &lt;cards&gt; is a JSON array consisting of cards for
 * the player in the position. See \ref jsoncardtype.
 *
 * \section jsondealstatecalls Calls JSON representation
 *
 * Calls made during the bidding are represented by a JSON array consisting of
 * the calls in the order they were made. Each call is a JSON object
 * consisting of the following:
 *
 * \code{.json}
 * {
 *     { "position": <position> },
 *     { "call": <call> }
 * }
 * \endcode
 *
 * - &lt;position&lt; is the position of the player who made the call. It has
 *   the same format as &lt;positionInTurn&gt;.
 * - &lt;call&gt; is the call made by the player. See \ref jsoncall.
 *
 * \section jsondealstatetrick Trick JSON representation
 *
 * Trick is represented by a JSON array consisting of the cards in the order
 * they were played. Each card is a JSON object consisting of the following:
 *
 * \code{.json}
 * {
 *     { "position": <position> },
 *     { "card": <card> }
 * }
 * \endcode
 *
 * - &lt;position&lt; is the position of the player who played the card. It has
 *   the same format as &lt;positionInTurn&gt;.
 * - &lt;card&gt; is the card played by the player. See \ref jsoncardtype.
 */

#include "bridge/DealState.hh"
#include "messaging/JsonSerializer.hh"

#include <string>

#ifndef MESSAGING_DEALSTATEJSONSERIALIZER_HH_
#define MESSAGING_DEALSTATEJSONSERIALIZER_HH_

namespace Bridge {
namespace Messaging {

/** \brief Key for DealState::stage in JSON object
 *
 * \sa jsondealstate
 */
extern const std::string DEAL_STATE_STAGE_KEY;

/** \brief Key for DealState::positionInTurn in JSON object
 *
 * \sa jsondealstate
 */
extern const std::string DEAL_STATE_POSITION_IN_TURN_KEY;

/** \brief Key for DealState::vulnerability in JSON object
 *
 * \sa jsondealstate
 */
extern const std::string DEAL_STATE_VULNERABILITY_KEY;

/** \brief Key for DealState::cards in JSON object
 *
 * \sa jsondealstate
 */
extern const std::string DEAL_STATE_CARDS_KEY;

/** \brief Key for DealState::declarer in JSON object
 *
 * \sa jsondealstate
 */
extern const std::string DEAL_STATE_DECLARER_KEY;

/** \brief Key for DealState::calls in JSON object
 *
 * \sa jsondealstate
 */
extern const std::string DEAL_STATE_CALLS_KEY;

/** \brief Key for positions in DealState::cards and DealState::calls in JSON
 * object
 *
 * \sa jsondealstate
 */
extern const std::string DEAL_STATE_POSITION_KEY;

/** \brief Key for calls in DealState::calls in JSON object
 *
 * \sa jsondealstate
 */
extern const std::string DEAL_STATE_CALL_KEY;

/** \brief Key for DealState::declarer in JSON object
 *
 * \sa jsondealstate
 */
extern const std::string DEAL_STATE_DECLARER_KEY;

/** \brief Key for DealState::contract in JSON object
 *
 * \sa jsondealstate
 */
extern const std::string DEAL_STATE_CONTRACT_KEY;

/** \brief Key for DealState::currentTrick in JSON object
 *
 * \sa jsondealstate
 */
extern const std::string DEAL_STATE_CURRENT_TRICK_KEY;

/** \brief Key for cards in DealState::currentTrick in JSON object
 *
 * \sa jsondealstate
 */
extern const std::string DEAL_STATE_CARD_KEY;

/** \brief Key for DealState::tricksWon in JSON object
 *
 * \sa jsondealstate
 */
extern const std::string DEAL_STATE_TRICKS_WON_KEY;

/** \brief JSON converter for Stage
 *
 * \sa JsonSerializer.hh, \ref jsondealstate
 */
template<>
struct JsonConverter<Stage>
{
    /** \brief Convert Stage to JSON
     */
    static nlohmann::json convertToJson(Stage stage);

    /** \brief Convert JSON to Stage
     */
    static Stage convertFromJson(const nlohmann::json& j);;
};

/** \brief JSON converter for DealState::Cards
 *
 * \sa JsonSerializer.hh, \ref jsondealstate
 */
template<>
struct JsonConverter<DealState::Cards>
{
    /** \brief Convert DealState::Cards to JSON
     */
    static nlohmann::json convertToJson(const DealState::Cards& cards);

    /** \brief Convert JSON to DealState::Cards
     */
    static DealState::Cards convertFromJson(const nlohmann::json& j);
};

/** \brief JSON converter for DealState::Calls
 *
 * \sa JsonSerializer.hh, \ref jsondealstate
 */
template<>
struct JsonConverter<DealState::Calls>
{
    /** \brief Convert DealState::Calls to JSON
     */
    static nlohmann::json convertToJson(const DealState::Calls& calls);

    /** \brief Convert JSON to DealState::Calls
     */
    static DealState::Calls convertFromJson(const nlohmann::json& j);
};

/** \brief JSON converter for DealState::Trick
 *
 * \sa JsonSerializer.hh, \ref jsondealstate
 */
template<>
struct JsonConverter<DealState::Trick>
{
    /** \brief Convert DealState::Trick to JSON
     */
    static nlohmann::json convertToJson(const DealState::Trick& trick);

    /** \brief Convert JSON to DealState::Trick
     */
    static DealState::Trick convertFromJson(const nlohmann::json& j);
};

/** \brief JSON converter for DealState
 *
 * \sa JsonSerializer.hh, \ref jsondealstate
 */
template<>
struct JsonConverter<DealState>
{
    /** \brief Convert DealState to JSON
     */
    static nlohmann::json convertToJson(const DealState& dealState);

    /** \brief Convert JSON to DealState
     */
    static DealState convertFromJson(const nlohmann::json& j);;
};

}
}

#endif // MESSAGING_DEALSTATEJSONSERIALIZER_HH_
