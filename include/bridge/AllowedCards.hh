/** \file
 *
 * \brief Definition of Bridge::getAllowedCards
 */

#ifndef ALLOWEDCARDS_H
#define ALLOWEDCARDS_H

#include "bridge/Card.hh"
#include "bridge/CardType.hh"
#include "bridge/Hand.hh"
#include "bridge/Trick.hh"
#include "Utility.hh"

namespace Bridge {

/** \brief Get allowed cards that can be played to trick
 *
 * Writes all cards that the hand in turn is allowed to play to \p trick to \p
 * out. No cards are allowed if the trick is completed.
 *
 * \param trick the trick to which the cards are played
 * \param out the output iterator the CardType objects are written to
 *
 * \return one past the position the last call was written to
 */
template<typename OutputIterator>
OutputIterator getAllowedCards(const Trick& trick, OutputIterator out)
{
    if (const auto hand = trick.getHandInTurn()) {
        for (const auto& card : *hand) {
            if (trick.canPlay(*hand, card)) {
                // Can play implies that the type of the card is known
                *out++ = dereference(card.getType());
            }
        }
    }
    return out;
}

}

#endif // ALLOWEDCARDS_H
