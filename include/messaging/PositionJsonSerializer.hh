/** \file
 *
 * \brief Definition of JSON serializer for Bridge::Position
 *
 * \page jsonposition Position JSON representation
 *
 * A Bridge::Position enumeration is represented by a JSON string that is either
 * "north", "east", "south" or "west".
 */

#ifndef MESSAGING_POSITIONJSONSERIALIZER_HH_
#define MESSAGING_POSITIONJSONSERIALIZER_HH_

#include <json.hpp>

namespace Bridge {

enum class Position;

/** \brief Convert Position to JSON
 */
void to_json(nlohmann::json&, Position);

/** \brief Convert JSON to Position
 */
void from_json(const nlohmann::json&, Position&);

}

#endif // MESSAGING_POSITIONJSONSERIALIZER_HH_
