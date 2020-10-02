/** \file
 *
 * \brief Definition of UUID generator utilities
 */

#ifndef UUIDGENERATOR_HH_
#define UUIDGENERATOR_HH_

#include "Random.hh"
#include "Uuid.hh"

#include <boost/uuid/random_generator.hpp>

namespace Bridge {

/** \brief The preferred UUID generator for the Bridge projecto
 *
 */
using UuidGenerator = boost::uuids::basic_random_generator<Rng>;

/** \brief Get reference to the global UUID generator
 *
 * \return Reference to the global UUID generator
 */
UuidGenerator& getUuidGenerator();

/** \brief Generate new UUID using the global generator
 *
 * \return A newly created UUID
 */
Uuid generateUuid();

}

#endif // UUIDGENERATOR_HH_
