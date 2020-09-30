/** \file
 *
 * \brief Definition of utilities used to enumerate card types
 */

#ifndef CARDTYPEITERATOR_HH_
#define CARDTYPEITERATOR_HH_

#include "bridge/CardType.hh"

#include <boost/iterator/counting_iterator.hpp>
#include <boost/iterator/transform_iterator.hpp>

namespace Bridge {

/** \brief Convert a card type to an integer
 *
 * This function is the inverse of enumerateCardType()
 *
 * \param card A card type
 *
 * \return An integer \c n such that <tt>enumerateCardType(n) == card</tt>
 */
int cardTypeIndex(const CardType& card);

/** \brief Convert an integer to a card type
 *
 * This function can be used to enumerate card types
 *
 * \param n number between 0 and 51
 *
 * \return card type corresponding to the number
 *
 * \throw std::invalid_argument if n is not valid card type
 */
CardType enumerateCardType(int n);

/** \brief Iterator for iterating over card types
 *
 * Cards are iterated in the same order as returned by enumerateCardType()
 * function.
 *
 * Example of creating a vector containing all cards:
 *
 * \code{.cc}
 * std::vector<CardType> cards(cardTypeIterator(0), cardTypeIterator(N_CARDS));
 * \endcode
 *
 * \sa enumerateCardType()
 */
inline auto cardTypeIterator(int n)
{
    return boost::make_transform_iterator(
        boost::make_counting_iterator(n), enumerateCardType);
}

}

#endif // CARDTYPEITERATOR_HH_
