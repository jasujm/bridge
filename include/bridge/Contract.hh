/** \file
 *
 * \brief Definition of Bridge::Contract struct
 */

#ifndef CONTRACT_HH_
#define CONTRACT_HH_

#include "bridge/Bid.hh"

#include "enhanced_enum/enhanced_enum.hh"

#include <boost/operators.hpp>

#include <iosfwd>
#include <string_view>

namespace Bridge {

/// \cond internal

/*[[[cog
import cog
import enumecg
import enum
class Doubling(enum.Enum):
  UNDOUBLED = "undoubled"
  DOUBLED = "doubled"
  REDOUBLED = "redoubled"
cog.out(enumecg.generate(Doubling, primary_type="enhanced"))
]]]*/
enum class DoublingLabel {
    UNDOUBLED,
    DOUBLED,
    REDOUBLED,
};

struct Doubling : ::enhanced_enum::enum_base<Doubling, DoublingLabel, std::string_view> {
    using ::enhanced_enum::enum_base<Doubling, DoublingLabel, std::string_view>::enum_base;
    static constexpr std::array values {
        value_type { "undoubled" },
        value_type { "doubled" },
        value_type { "redoubled" },
    };
};

constexpr Doubling enhance(DoublingLabel e) noexcept
{
    return e;
}

namespace Doublings {
inline constexpr const Doubling::value_type& UNDOUBLED_VALUE { std::get<0>(Doubling::values) };
inline constexpr const Doubling::value_type& DOUBLED_VALUE { std::get<1>(Doubling::values) };
inline constexpr const Doubling::value_type& REDOUBLED_VALUE { std::get<2>(Doubling::values) };
inline constexpr Doubling UNDOUBLED { DoublingLabel::UNDOUBLED };
inline constexpr Doubling DOUBLED { DoublingLabel::DOUBLED };
inline constexpr Doubling REDOUBLED { DoublingLabel::REDOUBLED };
}
///[[[end]]]

/// \endcond

/** \brief Number of tricks in a book
 *
 * \sa isMade()
 */
constexpr auto N_TRICKS_IN_BOOK = 6;

/** \brief A bridge contract
 *
 * Contract objects are equality comparable. They compare equal when both bid
 * and doubling statuses are equal.
 *
 * \note Boost operators library is used to ensure that operator!= is
 * generated with usual semantics when operator== is supplied.
 */
struct Contract : private boost::equality_comparable<Contract> {
    Bid bid;            ///< \brief The winning bid
    Doubling doubling;  ///< \brief The doubling status of the contract

    Contract() = default;

    /** \brief Create new contract
     *
     * \param bid the winning bid
     * \param doubling the doubling status of the contract
     */
    constexpr Contract(const Bid& bid, Doubling doubling) :
        bid {bid},
        doubling {doubling}
    {
    }
};

/** \brief Determine if declarer made the contract
 *
 * A contract is made if number of tricks is at least \ref N_TRICKS_IN_BOOK +
 * Bid::level.
 *
 * \param contract the contract
 * \param tricksWon the number of tricks won by the declarer
 *
 * \return true if the contract was made, false otherwise
 */
bool isMade(const Contract& contract, int tricksWon);

/** \brief Equality operator for contracts
 *
 * \sa Contract
 */
bool operator==(const Contract&, const Contract&);

/** \brief Output a Doubling to stream
 *
 * \param os the output stream
 * \param doubling the doubling status to output
 *
 * \return parameter \p os
 */
std::ostream& operator<<(std::ostream& os, Doubling doubling);

/** \brief Output a Contract to stream
 *
 * \param os the output stream
 * \param contract the contract to output
 *
 * \return parameter \p os
 */
std::ostream& operator<<(std::ostream& os, const Contract& contract);

}

#endif // CONTRACT_HH_
