/** \file
 *
 * \brief Definition of JSON serializer for Bridge::Partnership
 */

#ifndef MESSAGING_PARTNERSHIPJSONSERIALIZER_HH_
#define MESSAGING_PARTNERSHIPJSONSERIALIZER_HH_

#include "messaging/JsonSerializer.hh"

namespace Bridge {

enum class Partnership;

namespace Messaging {

/** \sa toJson() */
template<>
nlohmann::json toJson(const Partnership& partnership);

/** \sa fromJson() */
template<>
Partnership fromJson(const nlohmann::json& j);

}
}

#endif // MESSAGING_PARTNERSHIPJSONSERIALIZER_HH_
