/** \file
 *
 * \brief Definition of Bridge::Call variant and related concepts
 */

#ifndef CALL_HH_
#define CALL_HH_

// This is included for convenience, or otherwise using type Call will produce
// cryptic error messages if Bid is not available
#include "bridge/Bid.hh"

#include <boost/operators.hpp>
#include <boost/variant/variant.hpp>

#include <iosfwd>

namespace Bridge {

/** \brief Tag for bridge call pass
 */
struct Pass : private boost::totally_ordered<Pass> {};

/** \brief Tag for bridge call double
 */
struct Double : private boost::totally_ordered<Double> {};

/** \brief Tag for bridge call redouble
 */
struct Redouble : private boost::totally_ordered<Redouble> {};

/** \brief Bridge call
 *
 * A variant object representing a bridge call. A \ref Call object wraps either
 * Bid or any of the tags representing other calls (Pass, Double, Redouble).
 */
using Call = boost::variant<Pass, Bid, Double, Redouble>;

/** \brief Equality operator for Pass
 *
 * \return true
 */
inline bool operator==(Pass, Pass) { return true; }

/** \brief Less than operator for Pass
 */
inline bool operator<(Pass, Pass) { return false; }

/** \brief Equality operator for Double
 *
 * \return true
 */
inline bool operator==(Double, Double) { return true; }

/** \brief Less than operator for Double
 *
 * \return false
 */
inline bool operator<(Double, Double) { return false; }

/** \brief Equality operator for Redouble
 *
 * \return true
 */
inline bool operator==(Redouble, Redouble) { return true; }

/** \brief Less than operator for Redouble
 *
 * \return false
 */
inline bool operator<(Redouble, Redouble) { return false; }

/** \brief Output pass to stream
 *
 * The representation of a Pass tag is just the string “pass”.
 *
 * \note This function is required to generate proper streaming operator for the
 * variant type \ref Call.
 *
 * \param os the output stream
 *
 * \return parameter \p os
 */
std::ostream& operator<<(std::ostream& os, Pass);

/** \brief Output double to stream
 *
 * The representation of a Double tag is just the string “double”.
 *
 * \note This function is required to generate proper streaming operator for the
 * variant type \ref Call.
 *
 * \param os the output stream
 *
 * \return parameter \p os
 */
std::ostream& operator<<(std::ostream& os, Double);

/** \brief Output redouble to stream
 *
 * The representation of a Redouble tag is just the string “redouble”.
 *
 * \note This function is required to generate proper streaming operator for the
 * variant type \ref Call.
 *
 * \param os the output stream
 *
 * \return parameter \p os
 */
std::ostream& operator<<(std::ostream& os, Redouble);

}

#endif // CALL_HH_
