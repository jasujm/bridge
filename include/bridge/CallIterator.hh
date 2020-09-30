/** \file
 *
 * \brief Definition of utilities used to enumerate calls
 */

#ifndef CALLITERATOR_HH_
#define CALLITERATOR_HH_

#include "bridge/Call.hh"

#include <boost/iterator/counting_iterator.hpp>
#include <boost/iterator/transform_iterator.hpp>

namespace Bridge {

/** \brief Convert a call to an integer
 *
 * This function is the inverse of enumerateCall()
 *
 * \param call A call
 *
 * \return An integer \c n such that <tt>enumerateCall(n) == call</tt>
 */
int callIndex(const Call& call);

/** \brief Convert integer to a call
 *
 * This function can be used to enumerate calls
 *
 * \param n number between 0 and 37
 *
 * \return call corresponding to the number
 *
 * \throw std::invalid_argument if \c n is not a valid call
 */
Call enumerateCall(int n);

/** \brief Iterator for iterating over calls
 *
 * Calls are iterated in the same order as returned by enumerateCall() function.
 *
 * Example of creating a vector containing all calls:
 *
 * \code{.cc}
 * std::vector<Call> calls(callIterator(0), callIterator(N_CALLS));
 * \endcode
 *
 * \sa enumerateCall()
 */
inline auto callIterator(int n)
{
    return boost::make_transform_iterator(
        boost::make_counting_iterator(n), enumerateCall);
}

}

#endif // CALLITERATOR_HH_
