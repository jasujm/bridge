/** \file
 *
 * \brief Definition of JSON serializer for Bridge::TricksWon
 *
 * \page jsontrickswon TricksWon JSON representation
 *
 * A Bridge::TricksWon is represented by a JSON object consisting of the
 * following:
 *
 * \code{.json}
 * {
 *     { "northSouth": <northSouthTricksWon> },
 *     { "eastWest": <eastWestTricksWon> }
 * }
 * \endcode
 *
 * - &lt;northSouthTricksWon&gt; is an integer indicating the number of
 *   tricks won by the north–south partnership
 * - &lt;eastWestTricksWon&gt; is an integer indicating the number of
 *   tricks won by the east–west partnership
 */

#ifndef MESSAGING_TRICKSWONJSONSERIALIZER_HH_
#define MESSAGING_TRICKSWONJSONSERIALIZER_HH_

#include "messaging/JsonSerializer.hh"

namespace Bridge {

class TricksWon;

namespace Messaging {

/** \sa \ref jsontrickswon
 */
template<>
nlohmann::json toJson(const TricksWon& tricksWon);

/** \sa \ref jsontrickswon
 */
template<>
TricksWon fromJson(const nlohmann::json& j);

}
}

#endif // MESSAGING_TRICKSWONJSONSERIALIZER_HH_
