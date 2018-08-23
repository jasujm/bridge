/** \file
 *
 * \brief Definition of JSON serializer for Bridge::Partnership
 */

#ifndef MESSAGING_PARTNERSHIPJSONSERIALIZER_HH_
#define MESSAGING_PARTNERSHIPJSONSERIALIZER_HH_

#include <json.hpp>

namespace Bridge {

enum class Partnership;

/** \brief Convert Partnership to JSON
 */
void to_json(nlohmann::json&, Partnership);

/** \brief Convert JSON to Partnership
 */
void from_json(const nlohmann::json&, Partnership&);

}

#endif // MESSAGING_PARTNERSHIPJSONSERIALIZER_HH_
