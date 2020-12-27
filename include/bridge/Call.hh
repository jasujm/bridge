/** \file
 *
 * \brief Definition of Bridge::Call variant and related concepts
 */

#ifndef CALL_HH_
#define CALL_HH_

// This is included for convenience, or otherwise using type Call will produce
// cryptic error messages if Bid is not available
#include "bridge/Bid.hh"

#include <iosfwd>
#include <variant>

namespace Bridge {

/** \brief Tag for bridge call pass
 */
struct Pass {
    /// \brief Three‐way comparison
    constexpr auto operator<=>(const Pass&) const = default;
};

/** \brief Tag for bridge call double
 */
struct Double {
    /// \brief Three‐way comparison
    constexpr auto operator<=>(const Double&) const = default;
};

/** \brief Tag for bridge call redouble
 */
struct Redouble {
    /// \brief Three‐way comparison
    constexpr auto operator<=>(const Redouble&) const = default;
};

/** \brief Bridge call
 *
 * A variant object representing a bridge call. A \ref Call object wraps either
 * Bid or any of the tags representing other calls (Pass, Double, Redouble).
 */
using Call = std::variant<Pass, Bid, Double, Redouble>;

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
