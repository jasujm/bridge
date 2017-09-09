#ifndef UUID_HH_
#define UUID_HH_

#include <boost/uuid/uuid.hpp>

namespace Bridge {

/** \brief Alias for boost::uuids::uuid
 *
 * Boost UUID is the preferred UUID implementation in the Bridge project
 */
using Uuid = boost::uuids::uuid;

}

#endif // UUID_HH_
