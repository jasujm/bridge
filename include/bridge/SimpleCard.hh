/** \file
 *
 * \brief Definition of Bridge::SimpleCard class
 */

#ifndef SIMPLECARD_HH_
#define SIMPLECARD_HH_

#include "bridge/Card.hh"
#include "bridge/CardType.hh"

namespace Bridge {

/** \brief A playing card with immutable predefined card type
 *
 * The type of a SimpleCard is always known.
 */
class SimpleCard : public Card {
public:

    /** \brief Create new card
     *
     * \param cardType the type of the card
     */
    explicit SimpleCard(const CardType& cardType);

private:

    CardType handleGetType() const override;

    bool handleIsKnown() const override;

    CardType cardType;
};

}

#endif // SIMPLECARD_HH_
