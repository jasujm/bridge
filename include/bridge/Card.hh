/** \file
 *
 * \brief Definition of Bridge::Card interface
 */

#ifndef CARD_HH_
#define CARD_HH_

#include <boost/optional/optional_fwd.hpp>

namespace Bridge {

class CardType;

/** \brief Playing card
 */
class Card {
public:

    virtual ~Card();

    /** \brief Determine the type of the card
     *
     * \return CardType associated with this card, or none if the type is
     * unknown
     */
    boost::optional<CardType> getType() const;

    /** \brief Determine if this card is known
     *
     * In network game some cards are hidden to the current instance until
     * they are played or otherwise revealed.
     *
     * \return true if this card is known to the player, false otherwise
     */
    bool isKnown() const;

private:

    /** \brief Handle for returning CardType associated with this card
     *
     * It may be assumed that isKnown() == true.
     *
     * \sa getType()
     */
    virtual CardType handleGetType() const = 0;

    /** \brief Handle for returning whether this card is known to the player
     *
     * \sa isKnown()
     */
    virtual bool handleIsKnown() const = 0;
};

}

#endif // CARD_HH_
