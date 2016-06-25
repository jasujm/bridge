/** \file
 *
 * \brief Definition of Bridge::Engine::CardManager interface
 */

#ifndef ENGINE_CARDMANAGER_HH_
#define ENGINE_CARDMANAGER_HH_

#include "Observer.hh"
#include "Utility.hh"

#include <boost/optional/optional.hpp>
#include <boost/range/any_range.hpp>

#include <algorithm>
#include <cstddef>
#include <memory>

namespace Bridge {

class Hand;

namespace Engine {

/** \brief The link between the bridge engine and the underlying card
 * management protocol
 *
 * CardManager is an abstract interface BridgeEngine uses to manage the
 * playing cards. To maintain maximal flexibility in implementation of the
 * protocol, CardManager does not allow its client to take ownership of the
 * card objects it manages.
 */
class CardManager {
public:

    /** \brief Shuffling state
     */
    enum class ShufflingState {
        IDLE,       ///< Shuffling neither requested nor completed
        REQUESTED,  ///< Shuffling requested
        COMPLETED,  ///< Shuffling completed
    };

    virtual ~CardManager();

    /** \brief Subscribe to notifications about shuffling state
     *
     * The subscriber receives notifications whenever state of the card
     * manager changes.
     */
    void subscribe(std::weak_ptr<Observer<ShufflingState>> observer);

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
     *
     * Whether new shuffle request is initiated if the old one has not
     * completed is unspecified.
     */
    void requestShuffle();

    /** \brief Retrieve hand with the selected cards
     *
     * \tparam IndexIterator an input iterator which must return index when
     * dereferenced
     *
     * \param first iterator to the index of the first card
     * \param last iterator to one past the index of the last card
     *
     * \return the hand containing references to the selected cards, or
     * nullptr if the shuffling is not completed
     *
     * \throw std::out_of_range if any index is out of range
     */
    template<typename IndexIterator>
    std::unique_ptr<Hand> getHand(IndexIterator first, IndexIterator last);

    /** \brief Determine if the deck is shuffled
     *
     * \return true if shuffling is completed, false otherwise
     *
     * \note If the shuffling has not been requested for the first time, this
     * method always returns false.
     */
    bool isShuffleCompleted() const;

    /** \brief Determine the number of cards available
     *
     * \return the number of cards managed by this object, or none if
     * isShuffleCompleted() == false
     */
    boost::optional<std::size_t> getNumberOfCards() const;

protected:

    /** Forward traversable range of indices
     */
    using IndexRange = boost::any_range<
        std::size_t, boost::forward_traversal_tag, std::size_t>;

private:

    /** \brief Handle subscribing to shuffling state notificatons
     *
     * \sa subscribe()
     */
    virtual void handleSubscribe(
        std::weak_ptr<Observer<ShufflingState>> observer) = 0;

    /* \brief Handle for requesting that cards be shuffled
     *
     * \sa requestShuffle()
     */
    virtual void handleRequestShuffle() = 0;

    /** \brief Handle for returning a hand
     *
     * It may be assumed that isShuffleCompleted() == true and ns[n] <
     * getNumberOfCards() for each n.
     *
     * \param ns list of indices
     *
     * \sa getHand()
     */
    virtual std::unique_ptr<Hand> handleGetHand(IndexRange ns) = 0;

    /** \brief Handle for determining if the shuffle is completed
     *
     * \return true if the shuffle is completed, false otherwise
     *
     * \sa isShuffleCompleted()
     */
    virtual bool handleIsShuffleCompleted() const = 0;

    /** \brief Handle for returning total number of cards
     *
     * It may be assumed that isShuffleCompleted() == true
     *
     * \sa getNumberOfCards()
     */
    virtual std::size_t handleGetNumberOfCards() const = 0;
};

template<typename IndexIterator>
std::unique_ptr<Hand> CardManager::getHand(
    IndexIterator first, IndexIterator last)
{
    if (const auto n_cards = getNumberOfCards()) {
        std::for_each(
            first, last,
            [n_cards = *n_cards](const auto n) {
                checkIndex(n, n_cards);
            });
        return handleGetHand(IndexRange(first, last));
    }
    return nullptr;
}

}
}

#endif // ENGINE_CARDMANAGER_HH_
