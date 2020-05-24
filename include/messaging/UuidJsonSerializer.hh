/** \file
 *
 * \brief Definition of JSON serializer for UUID
 */

#ifndef MESSAGING_UUIDJSONSERIALIZER_HH_
#define MESSAGING_UUIDJSONSERIALIZER_HH_

#include <nlohmann/json.hpp>

#include "bridge/Uuid.hh"

namespace nlohmann {

/** \brief Explicit specialization of adl_serializer for Uuid
 */
template<>
struct adl_serializer<Bridge::Uuid> {

    /** \brief Convert UUID to JSON
     */
    static void to_json(json&, const Bridge::Uuid&);

    /** \brief Convert JSON to UUID
     */
    static void from_json(const json&, Bridge::Uuid&);

};

}

#endif // MESSAGING_UUIDJSONSERIALIZER_HH_
