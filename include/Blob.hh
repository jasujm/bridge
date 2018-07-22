/** \file
 *
 * Definition of Bridge::Blob
 */

#ifndef BLOB_HH_
#define BLOB_HH_

#include <cstddef>
#include <vector>

namespace Bridge {

/** \brief The preferred data type for holding arbitrary length binary data
 */
using Blob = std::vector<std::byte>;

}

#endif // Bridge
