/** \file
 *
 * \brief Utilities for shuffling deck
 */

#ifndef CARDSHUFFLE_HH_
#define CARDSHUFFLE_HH_

#include <vector>

namespace Bridge {

struct CardType;

/** \brief Generate a deck of randomly shuffled cards
 *
 * \return Vector containing all 52 distinct CardType values in random order
 */
std::vector<CardType> generateShuffledDeck();

}

#endif // CARDSHUFFLE_HH_
