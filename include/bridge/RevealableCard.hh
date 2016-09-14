/** \file
 *
 * \brief Definition of Bridge::RevealableCard class
 */

#ifndef REVEALABLECARD_HH_
#define REVEALABLECARD_HH_

#include "bridge/Card.hh"
#include "bridge/CardType.hh"

#include <boost/optional/optional.hpp>

namespace Bridge {

/** \brief A playing card that can be revealed during its lifetime
 *
 * RevealableCard models a card whose type is initially unknown (that is,
 * after creation isKnown() == false). Once revealed, the type is known for
 * the remaining of the lifetime of the card.
 */
class RevealableCard : public Card {
public:

    /** \brief Reveal the card
     *
     * After the method is called, isKnown() == true and \c getType() == \p
     * cardType
     */
    void reveal(const CardType& cardType);

private:

    CardType handleGetType() const override;

    bool handleIsKnown() const override;

    boost::optional<CardType> cardType;
};

}

#endif // REVEALABLECARD_HH_
