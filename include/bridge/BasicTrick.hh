/** \file
 *
 * \brief Definition of Bridge::BasicTrick class
 */

#ifndef BASICTRICK_HH_
#define BASICTRICK_HH_

#include "bridge/CardType.hh"
#include "bridge/Trick.hh"

#include <boost/core/noncopyable.hpp>
#include <boost/optional/optional.hpp>

#include <functional>
#include <stdexcept>
#include <vector>

namespace Bridge {

/** \brief Concrete implementation of the Trick interface
 *
 * BasicTrick maintains turns based on predetermined list of hands. It
 * enforces standard contract bridge rules. In addition it doesn’t allow new
 * suit to be played to trick if it is indeterminate whether the hand is out
 * of the suit.
 */
class BasicTrick : public Trick, private boost::noncopyable {
public:

    /** \brief Create new trick
     *
     * The constructor takes a list of hands. The order of the hands will also
     * be the order in which they get turns.
     *
     * \note This class borrows reference to the hands. It is the
     * responsibility of the client of this class to ensure that the lifetime
     * of the hand objects exceeds the lifetime of the constructed trick
     * object.
     *
     * \note There must be exactly four hands.
     *
     * \tparam HandIterator an input iterator which must return a reference to
     * a Hand object when dereferenced
     *
     * \param first the first hand
     * \param last the last hand
     * \param trump optional trump suit, or none if trick is no trump
     *
     * \throw std::invalid_argument if the number of hands is not four
     */
    template<typename HandIterator>
    BasicTrick(HandIterator first, HandIterator last,
               boost::optional<Suit> trump = boost::none);

    virtual ~BasicTrick();

private:

    void handleAddCardToTrick(const Card& card) override;

    std::size_t handleGetNumberOfCardsPlayed() const override;

    const Card& handleGetCard(std::size_t n) const override;

    const Hand& handleGetHand(std::size_t n) const override;

    bool handleIsPlayAllowed(const Hand& hand,
                             const Card& card) const override;

    const Hand& handleGetWinner() const override;

    const std::vector<std::reference_wrapper<const Hand>> hands;
    const boost::optional<Suit> trump;
    std::vector<std::reference_wrapper<const Card>> cards;
};

template<typename HandIterator>
BasicTrick::BasicTrick(HandIterator first, HandIterator last,
                       boost::optional<Suit> trump) :
    hands(first, last),
    trump {trump}
{
    if (hands.size() != N_CARDS_IN_TRICK) {
        throw std::invalid_argument("Invalid number of hands");
    }
}

}

#endif // BASICTRICK_HH_
