/** \file
 *
 * \file The common random number generator
 */

#ifndef RANDOM_HH_
#define RANDOM_HH_

#include <random>

namespace Bridge {

/** \brief The preferred random number generator for the Bridge project
 */
using Rng = std::mt19937;

/** \brief Get reference to the global random number generator
 *
 * \return Reference to the global random number generator
 */
Rng& getRng();

}

#endif // RANDOM_HH_
