/** \file
 *
 * \brief Definition of Bridge::BasicTrick class
 */

#ifndef BASICTRICK_HH_
#define BASICTRICK_HH_

#include "bridge/CardType.hh"
#include "bridge/Trick.hh"

#include <boost/core/noncopyable.hpp>

#include <functional>
#include <stdexcept>
#include <vector>

namespace Bridge {

/** \brief Concrete implementation of the Trick interface
 *
 * BasicTrick keeps list of cards played to the trick and maintains turns
 * based on predetermined list of hands.
 */
class BasicTrick : public Trick, private boost::noncopyable {
public:

    /** \brief Create new trick
     *
     * The constructor takes a list of hands. The order of the hands will also
     * be the order in which they get turns.
     *
     * \note The BasicTrick object borrows references to the hands. It is the
     * responsibility of the client of this class to ensure that the lifetime
     * of the hand objects exceeds the lifetime of the constructed trick
     * object.
     *
     * \note There must be exactly four hands.
     *
     * \tparam HandIterator an input iterator which must return a constant
     * reference to a Hand object when dereferenced
     *
     * \param first iterator to the firts hand to play to the trick
     * \param last iterator one past the last hand to play to the trick
     *
     * \throw std::invalid_argument if the number of hands is not four
     */
    template<typename HandIterator>
    BasicTrick(HandIterator first, HandIterator last);

    virtual ~BasicTrick();

private:

    void handleAddCardToTrick(const Card& card) override;

    std::size_t handleGetNumberOfCardsPlayed() const override;

    const Card& handleGetCard(std::size_t n) const override;

    const Hand& handleGetHand(std::size_t n) const override;

    const std::vector<std::reference_wrapper<const Hand>> hands;
    std::vector<std::reference_wrapper<const Card>> cards;
};

template<typename HandIterator>
BasicTrick::BasicTrick(HandIterator first, HandIterator last) :
    hands(first, last)
{
    if (hands.size() != N_CARDS_IN_TRICK) {
        throw std::invalid_argument("Invalid number of hands");
    }
}

}

#endif // BASICTRICK_HH_
