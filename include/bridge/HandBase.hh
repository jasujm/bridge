/** \file
 *
 * \brief Definition of Bridge::HandBase class
 */

#ifndef HANDBASE_HH_
#define HANDBASE_HH_

#include "bridge/Hand.hh"

#include <functional>
#include <vector>

namespace Bridge {

/** \brief Abstract base class for hands
 *
 * HandBase implements storing and tracking playing of cards. It is up to the
 * derived class to implement the policy for revealing cards in the hand.
 */
class HandBase :
    public Hand, private Hand::CardRevealStateObserver::ObservableType {
public:

    using Hand::subscribe;

protected:

    /** \brief Create new hand
     *
     * Create new hand holding cards specified by the range given as as
     * arguments to the constructor.
     *
     * \note The HandBase object borrows references to the cards. It is the
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
    HandBase(CardIterator first, CardIterator last);

    /** \brief Notify all subscribers about card reveal state
     *
     * The base class stores the notifier for card reveal states. Derived
     * classes can use this method to access the notifier.
     */
    using CardRevealStateObserver::ObservableType::notifyAll;

private:

    struct CardEntry {
        std::reference_wrapper<const Card> card;
        bool isPlayed;

        CardEntry(const Card& card);
    };

    void handleSubscribe(
        std::weak_ptr<CardRevealStateObserver> observer) override;

    void handleMarkPlayed(std::size_t n) override;

    const Card& handleGetCard(std::size_t n) const override;

    bool handleIsPlayed(std::size_t n) const override;

    std::size_t handleGetNumberOfCards() const override;

    std::vector<CardEntry> cards;
};

template<typename CardIterator>
HandBase::HandBase(CardIterator first, CardIterator last) :
    cards(first, last)
{
}

}

#endif // HANDBASE_HH_
