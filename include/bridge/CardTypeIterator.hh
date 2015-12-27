/** \file
 *
 * \brief Definition of utilities used to enumerate card types
 */

#ifndef CARDTYPEITERATOR_HH_
#define CARDTYPEITERATOR_HH_

#include "bridge/CardType.hh"

#include <boost/iterator/counting_iterator.hpp>
#include <boost/iterator/transform_iterator.hpp>

#include <cstddef>

namespace Bridge {

/** \brief Convert integer into a card type
 *
 * This function can be used to enumerate card types
 *
 * \param n number between 0 and 51
 *
 * \return card type corresponding to the number
 *
 * \throw std::domain_error if n is not valid card type
 */
CardType enumerateCardType(std::size_t n);

/** \brief Iterator for iterating over card types
 *
 * Cards are iterated in the same order as returned by enumerateCardType()
 * function.
 *
 * Example for creating vector containing all cards:
 *
 * \code{.cc}
 * std::vector<CardType> cards(cardTypeIterator(0), cardTypeIterator(N_CARDS));
 * \endcode
 *
 * \sa enumerateCardType()
 */
inline auto cardTypeIterator(std::size_t n)
{
    return boost::make_transform_iterator(
        boost::make_counting_iterator(n), enumerateCardType);
}

}

#endif // CARDTYPEITERATOR_HH_
