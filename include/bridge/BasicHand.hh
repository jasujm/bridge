/** \file
 *
 * \brief Definition of Bridge::BasicHand class
 */

#ifndef BASICHAND_HH_
#define BASICHAND_HH_

#include "bridge/Hand.hh"

#include <boost/core/noncopyable.hpp>

#include <algorithm>
#include <functional>
#include <vector>

namespace Bridge {

/** \brief Concrete implementation of the Hand interface
 */
class BasicHand : public Hand, private boost::noncopyable {
public:

    /** \brief Create new hand
     *
     * Create new hand holding cards specified by the range given as as
     * arguments to the constructor.
     *
     * \note The BasicHand object borrows references to the cards. It is the
     * resposibility of the client of this class to ensure that the lifetime
     * of the card objects exceeds the lifetime of the constructed hand.
     *
     * \tparam CardIterator an input iterator that must return a constant
     * reference to a Card object when dereferenced
     *
     * \param first iterator to the first card to be dealt
     * \param last iterator one past the last card to be dealt
     */
    template<typename CardIterator>
    BasicHand(CardIterator first, CardIterator last);

    /** \brief Reveal cards
     *
     * Notify the card reveal state observers that cards in given range are
     * revealed. Unless all cards in the range are known, this method does
     * nothing.
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

    struct CardEntry {
        std::reference_wrapper<const Card> card;
        bool isPlayed;

        CardEntry(const Card& card);
    };

    void handleSubscribe(
        std::weak_ptr<CardRevealStateObserver> observer) override;

    void handleRequestReveal(const IndexVector& ns) override;

    void handleMarkPlayed(int n) override;

    const Card& handleGetCard(int n) const override;

    bool handleIsPlayed(int n) const override;

    int handleGetNumberOfCards() const override;

    std::vector<CardEntry> cards;
    CardRevealStateObserver::ObservableType notifier;
};

template<typename CardIterator>
BasicHand::BasicHand(CardIterator first, CardIterator last) :
    cards(first, last)
{
}

template<typename IndexIterator>
bool BasicHand::reveal(IndexIterator first, IndexIterator last)
{
    const auto notify = std::all_of(
        first, last, [this](const auto n)
        {
            return cards.at(n).card.get().isKnown();
        });
    if (notify) {
        notifier.notifyAll(
            CardRevealState::COMPLETED, IndexVector(first, last));
    }
    return notify;
}

}

#endif // BASICHAND_HH_
