/** \file
 *
 * \brief Definition of Bridge::Engine::CardManager interface
 */

#ifndef ENGINE_CARDMANAGER_HH_
#define ENGINE_CARDMANAGER_HH_

#include "Observer.hh"
#include "Utility.hh"

#include <cstddef>
#include <memory>
#include <vector>

namespace Bridge {

class Hand;

namespace Engine {

/** \brief Tag for announcing that shuffling is completed
 *
 * \sa CardManager
 */
struct Shuffled {};

/** \brief The link between the bridge engine and the underlying card
 * management protocol
 *
 * CardManager is an abstract interface BridgeEngine uses to manage the
 * playing cards. To maintain maximal flexibility in implementation of the
 * protocol, CardManager does not allow its client to take ownership of the
 * card objects it manages.
 */
class CardManager : public Observable<Shuffled> {
public:

    virtual ~CardManager();

    /** \brief Subscribe to notification announced when the cards are shuffled
     */
    using Observable<Shuffled>::subscribe;

    /** \brief Request that the deck be (re)shuffled
     *
     * The call may be asynchronous so the new deck doesn’t need to be
     * immediately available after this call returns. The status of shuffle
     * can be queried with isShuffleCompleted(). The CardManager notifies its
     * observers when the shuffling is completed.
     *
     * A call to requestShuffle invalidates any references to Card objects the
     * client of this class may have retrieved using getHand(). It is the
     * responsibility of the client to ensure that the lifetime of CardManager
     * exceeds the lifetime of the hands.
     */
    void requestShuffle();

    /** \brief Retrieve hand with the selected cards
     *
     * \tparam IndexIterator an input iterator which must return index when
     * dereferenced
     *
     * \param first the index of the first card
     * \param last the index of the last card
     *
     * \return the hand containing references to the selected cards
     *
     * \throw std::out_of_range if any index is out of range
     */
    template<typename IndexIterator>
    std::unique_ptr<Hand> getHand(IndexIterator first, IndexIterator last);

    /** \brief Determine if the deck is shuffled
     *
     * This is equivalent to getNumberOfCards() > 0.
     *
     * \return getNumberOfCards() > 0
     */
    bool isShuffleCompleted() const;

    /** \brief Determine the number of cards available
     *
     * \return the number of cards managed by this object
     */
    std::size_t getNumberOfCards() const;

private:

    /* \brief Handle for requesting that cards be shuffled
     *
     * \sa requestShuffle()
     */
    virtual void handleRequestShuffle() = 0;

    /** \brief Handle for returning a hand
     *
     * It may be assumed that ns[n] < getNumberOfCards() for each n.
     *
     * \param ns list of indices
     *
     * \sa getHand()
     */
    virtual std::unique_ptr<Hand> handleGetHand(
        const std::vector<std::size_t>& ns) = 0;

    /** \brief Handle for returning total number of cards
     *
     * \sa getNumberOfCards()
     */
    virtual std::size_t handleGetNumberOfCards() const = 0;
};

template<typename IndexIterator>
std::unique_ptr<Hand> CardManager::getHand(
    IndexIterator first, IndexIterator last)
{
    const auto n_cards = handleGetNumberOfCards();
    auto ns = std::vector<std::size_t> {};
    for (; first != last; ++first) {
        const auto n = *first;
        ns.emplace_back(checkIndex(n, n_cards));
    }
    return handleGetHand(ns);
}

}
}

#endif // ENGINE_CARDMANAGER_HH_
