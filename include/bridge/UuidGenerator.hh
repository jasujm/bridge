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

/** \brief Return a newly created UUID generator
 */
UuidGenerator createUuidGenerator();

}

#endif // UUIDGENERATOR_HH_
