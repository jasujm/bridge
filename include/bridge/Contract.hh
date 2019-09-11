/** \file
 *
 * \brief Definition of Bridge::Contract struct
 */

#ifndef CONTRACT_HH_
#define CONTRACT_HH_

#include "bridge/Bid.hh"

#include <boost/bimap/bimap.hpp>
#include <boost/operators.hpp>

#include <array>
#include <iosfwd>
#include <string>

namespace Bridge {

/** \brief Doubling status of a bridge contract
 */
enum class Doubling {
    UNDOUBLED,
    DOUBLED,
    REDOUBLED
};

/** \brief Number of doublings
 *
 * \sa Doubling
 */
constexpr auto N_DOUBLINGS = 3;

/** \brief Array containing all doublings
 *
 * \sa Doubling
 */
constexpr std::array<Doubling, N_DOUBLINGS> DOUBLINGS {
    Doubling::UNDOUBLED,
    Doubling::DOUBLED,
    Doubling::REDOUBLED,
};

/** \brief Type of \ref DOUBLING_TO_STRING_MAP
 */
using DoublingToStringMap = boost::bimaps::bimap<Doubling, std::string>;

/** \brief Two-way map between Doubling enumerations and their string
 * representation
 */
extern const DoublingToStringMap DOUBLING_TO_STRING_MAP;

/** \brief Number of tricks in a book
 *
 * \sa isMade()
 */
constexpr int N_TRICKS_IN_BOOK = 6;

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
