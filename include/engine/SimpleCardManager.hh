/** \file
 *
 * \brief Definition of Bridge::Engine::SimpleCardManager class
 */

#ifndef ENGINE_SIMPLECARDMANAGER_HH_
#define ENGINE_SIMPLECARDMANAGER_HH_

#include "engine/CardManager.hh"

#include <boost/core/noncopyable.hpp>

#include <memory>
#include <vector>

namespace Bridge {

class CardType;

namespace Engine {

/** \brief A simple card manager
 *
 * SimpleCardManager can be used to manage cards in simple protocol where the
 * permutation of the cards is negotiated once at the beginning of the deal
 * and is known to all peers (including the case where the bridge application
 * works as server without peers and controls the whole game). Because it does
 * not support hiding information about the cards owned by one particular
 * peer, it is only suitable for social games between parties that trust each
 * other.
 *
 * SimpleCardManager is a state machine with three states:
 *
 * - Initially the card manager is in “idle” state
 * - When shuffle is requested using requestShuffle(), the card manager is in
 *   “shuffle requested” state. Any hands retrieved from the card manager
 *   earlier are invalidated and new hands cannot be retrieved before
 *   completing the shuffle.
 * - When cards are shuffled using shuffle(), the card manager is in “shuffle
 *   completed” state. Hands determined by the cards added by shuffle() call
 *   can be retrieved as hands.
 */
class SimpleCardManager : public CardManager, private boost::noncopyable {
public:

    /** \brief Create new card manager
     */
    SimpleCardManager();

    /** \brief Create new card manager with initial cards
     *
     * This constructor creates a card manager directly in shuffled state, as if
     * by first requesting a shuffle and then calling <tt>shuffle(first,
     * last)</tt>.
     *
     * \tparam CardTypeIterator An input iterator that, when derefernced,
     * returns and object convertible to CardType.
     *
     * \param first iterator to the first card
     * \param last iterator one past the last card
     */
    template<typename CardTypeIterator>
    SimpleCardManager(CardTypeIterator first, CardTypeIterator last);

    ~SimpleCardManager();

    /** \brief Add shuffled cards
     *
     * After shuffling is requested, this method can be called to complete the
     * shuffle. It has no effect in “idle” or “shuffle completed” states.
     *
     * \tparam CardTypeIterator An input iterator that, when derefernced,
     * returns and object convertible to CardType.
     *
     * \param first iterator to the first card
     * \param last iterator one past the last card
     */
    template<typename CardTypeIterator>
    void shuffle(CardTypeIterator first, CardTypeIterator last);

    class Impl;

private:

    using CardTypeVector = std::vector<CardType>;

    void handleSubscribe(
        std::weak_ptr<Observer<ShufflingState>> observer) override;

    void handleRequestShuffle() override;

    std::shared_ptr<Hand> handleGetHand(const IndexVector& ns) override;

    bool handleIsShuffleCompleted() const override;

    int handleGetNumberOfCards() const override;

    const Card& handleGetCard(int n) const override;

    void internalShuffle(const CardTypeVector& cards);

    const std::unique_ptr<Impl> impl;
};

template<typename CardTypeIterator>
SimpleCardManager::SimpleCardManager(
    CardTypeIterator first, CardTypeIterator last) :
    SimpleCardManager {}
{
    handleRequestShuffle();
    internalShuffle(CardTypeVector(first, last));
}

template<typename CardTypeIterator>
void SimpleCardManager::shuffle(CardTypeIterator first, CardTypeIterator last)
{
    internalShuffle(CardTypeVector(first, last));
}

}
}

#endif // ENGINE_SIMPLECARDMANAGER_HH_
