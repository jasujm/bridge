/** \file
 *
 * \brief Definition of Bridge::BlobMap
 */

#ifndef BLOBMAP_HH_
#define BLOBMAP_HH_

#include <map>

#include "Blob.hh"

namespace Bridge {

/** \brief Comparator for blobs
 *
 * An instance of BytewiseCompare can be used to establish order of keys byte by
 * byte.  It is useful for maps and algorithms that operate on object
 * representations without regarding the interpretation of the bytes.
 *
 * \sa BlobMap
 */
struct BytewiseCompare {

    /** \brief Enable zero copy optimizations with STL containers
     */
    using is_transparent = void;

    /** \brief Compare byte by byte
     *
     * \param lhs, rhs
     *
     * \return true if the object representation of \p lhs is lexicographically
     * smaller than the object representation of \p rhs
     */
    template<typename Container1, typename Container2>
    bool operator()(const Container1& lhs, const Container2& rhs) const
    {
        return asBytes(lhs) < asBytes(rhs);
    }
};

/** \brief Map type with blob keys
 *
 * BlobMap is a STL map from Blob type to a type \c T. It uses BytewiseCompare
 * to allow an object of any byte oriented type to be used as key without making
 * copies.
 */
template<
    typename T,
    typename Allocator = typename std::map<Blob, T>::allocator_type>
using BlobMap = std::map<Blob, T, BytewiseCompare, Allocator>;

}

#endif
