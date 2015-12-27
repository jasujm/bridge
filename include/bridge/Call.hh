/** \file
 *
 * \brief Definition of Bridge::Call variant and related concepts
 */

#ifndef CALL_HH_
#define CALL_HH_

// This is included for convenience, or otherwise using type Call will produce
// cryptic error messages if Bid is not available
#include "bridge/Bid.hh"

#include <boost/variant/variant.hpp>

#include <iosfwd>

namespace Bridge {

/** \brief Tag for bridge call pass
 */
struct Pass {};

/** \brief Tag for bridge call double
 */
struct Double {};

/** \brief Tag for bridge call redouble
 */
struct Redouble {};

/** \brief Bridge call
 *
 * A call is a discriminated union of either Bid or any of the tags
 * representing other calls (pass, double, redouble). See
 * http://www.boost.org/doc/libs/1_60_0/doc/html/variant.html for
 * documentation on boost::variant used to implement the call.
 */
using Call = boost::variant<Pass, Bid, Double, Redouble>;

/** \brief Compare two Pass objects for equality
 *
 * Two Pass objects always compare equal. This function is required to
 * generate operator== for \ref Call.
 *
 * \return true
 */
inline bool operator==(Pass, Pass) { return true; }

/** \brief Compare two Pass objects for inequality
 *
 * Two Pass objects always compare equal. This function is required to
 * generate operator== for \ref Call.
 *
 * \return false
 */
inline bool operator!=(Pass, Pass) { return false; }

/** \brief Compare two Double objects for equality
 *
 * Two Double objects always compare equal. This function is required to
 * generate operator!= for \ref Call.
 *
 * \return true
 */
inline bool operator==(Double, Double) { return true; }

/** \brief Compare two Double objects for inequality
 *
 * Two Double objects always compare equal. This function is required to
 * generate operator!= for \ref Call.
 *
 * \return false
 */
inline bool operator!=(Double, Double) { return false; }

/** \brief Compare two Redouble objects for equality
 *
 * Two Redouble objects always compare equal. This function is required to
 * generate operator== for \ref Call.
 *
 * \return true
 */
inline bool operator==(Redouble, Redouble) { return true; }

/** \brief Compare two Redouble objects for inequality
 *
 * Two Redouble objects always compare equal. This function is required to
 * generate operator!= for \ref Call.
 *
 * \return false
 */
inline bool operator!=(Redouble, Redouble) { return false; }

/** \brief Output pass to stream
 *
 * This function is required to generator operator<< for \ref Call.
 *
 * \param os the output stream
 *
 * \return parameter \p os
 */
std::ostream& operator<<(std::ostream& os, Pass);

/** \brief Output double to stream
 *
 * This function is required to generator operator<< for \ref Call.
 *
 * \param os the output stream
 *
 * \return parameter \p os
 */
std::ostream& operator<<(std::ostream& os, Double);

/** \brief Output redouble to stream
 *
 * This function is required to generator operator<< for \ref Call.
 *
 * \param os the output stream
 *
 * \return parameter \p os
 */
std::ostream& operator<<(std::ostream& os, Redouble);

}

#endif // CALL_HH_
