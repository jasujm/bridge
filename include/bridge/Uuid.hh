/** \file
 *
 * \brief Definition of Bridge::Uuid
 */
#ifndef UUID_HH_
#define UUID_HH_

#include <boost/uuid/uuid.hpp>

namespace Bridge {

/** \brief The preferred UUID implementation in the Bridge project
 */
using Uuid = boost::uuids::uuid;

}

#endif // UUID_HH_
