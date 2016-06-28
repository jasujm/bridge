/** \file
 *
 * \brief Definition of Bridge::BasicHand class
 */

#ifndef BASICHAND_HH_
#define BASICHAND_HH_

#include "bridge/HandBase.hh"

#include <boost/core/noncopyable.hpp>

#include <algorithm>

namespace Bridge {

/** \brief Concrete implementation of the Hand interface
 *
 * BasicHand can hold any types of cards. It depends on an external class to
 * reveal the cards.
 */
class BasicHand : public HandBase, private boost::noncopyable {
public:

    using HandBase::HandBase;

    /** \brief Reveal cards
     *
     * Notify the card reveal state observers that cards in given range are
     * revealed. Unless all cards in the range are known or already played,
     * this method does nothing.
     *
     * \note The BasicHand object does not track requests. It is expected,
     * though not required, that this method is called after calling
     * requestReveal().
     *
     * \tparam IndexIterator A forward iterator that, when dereferenced,
     * returns an integer.
     *
     * \param first iterator to the first index to be revealed
     * \param last iterator the the last index to be revealed
     *
     * \return true if all cards in the range are known and the call causes
     * the observers to be notified, false otherwise
     *
     * \throw std::out_of_range if n >= getNumberOfCards() for some n in the
     * range given
     */
    template<typename IndexIterator>
    bool reveal(IndexIterator first, IndexIterator last);

private:

    void handleRequestReveal(IndexRange ns) override;
};

template<typename IndexIterator>
bool BasicHand::reveal(IndexIterator first, IndexIterator last)
{
    const auto notify = std::all_of(
        first, last, [this](const auto n)
        {
            const auto card = this->getCard(n);
            return !card || card->isKnown();
        });
    if (notify) {
        notifyAll(CardRevealState::COMPLETED, IndexRange(first, last));
    }
    return notify;
}

}

#endif // BASICHAND_HH_
