/** \file
 *
 * \brief Definition of Bridge::Hand interface
 */

#ifndef HAND_HH_
#define HAND_HH_

#include "bridge/Card.hh"
#include "Observer.hh"

#include <boost/iterator/counting_iterator.hpp>
#include <boost/iterator/filter_iterator.hpp>
#include <boost/iterator/indirect_iterator.hpp>
#include <boost/iterator/transform_iterator.hpp>
#include <boost/logic/tribool_fwd.hpp>

#include <memory>
#include <optional>
#include <stdexcept>
#include <vector>

namespace Bridge {

struct CardType;
class Player;
struct Suit;

/** \brief A bridge hand
 */
class Hand
{
public:

    /** \brief Card reveal state
     */
    enum class CardRevealState {
        REQUESTED,  ///< Revealing card requested
        COMPLETED,  ///< Revealing card completed
    };

    /** \brief Vector of indices
     */
    using IndexVector = std::vector<int>;

    /** \brief Observer of card reveal state
     *
     * \sa subscribe()
     */
    using CardRevealStateObserver = Observer<CardRevealState, IndexVector>;

    virtual ~Hand();

    /** \brief Subscribe to notifications about card reveal state
     *
     * The subscriber receives notifications whenever card request is
     * revealed. The first parameter of the notification is the state of the
     * request (reveal requested or completed). The second parameter is the
     * range of indices of the cards (to be) revealed.
     */
    void subscribe(std::weak_ptr<CardRevealStateObserver> observer);

    /** \brief Request that cards in the hand are revealed
     *
     * When request is revealed, all observers are first notified about the
     * new request. After completion of reveal is notified, it is guaranteed
     * that the cards revealed are known to all players.
     *
     * Whether new reveal request is initiated if the old one has not
     * completed is unspecified.
     *
     * \tparam IndexIterator A forward iterator that, when dereferenced,
     * returns an integer.
     *
     * \param first iterator to the first index to be revealed
     * \param last iterator the the last index to be revealed
     *
     * \throw std::out_of_range if n >= getNumberOfCards() for some n in the
     * range given
     */
    template<typename IndexIterator>
    void requestReveal(IndexIterator first, IndexIterator last);

    /** \brief Mark card as played.
     *
     * Does nothing if the card has already been played or the card type is
     * unknown.
     *
     * \param n the index of the card to be played
     */
    void markPlayed(int n);

    /** \brief Retrieve a card
     *
     * \note Note that index or cards do not change after they have been
     * dealt, so card m > n will be returned by call getCard(m) even if card n
     * has previously been played.
     *
     * \param n index of the card to be retrieved
     *
     * \return pointer to the nth card, or nullptr if it has already been
     * played
     *
     * \throw std::out_of_range if n >= getNumberOfCards()
     */
    const Card* getCard(int n) const;

    /** \brief Determine if a card has been played
     *
     * \param n index of the card
     *
     * \return true if the card has been played, false otherwise
     *
     * \throw std::out_of_range if n >= getNumberOfCards()
     */
    bool isPlayed(int n) const;

    /** \brief Determine number of cards dealt to the hand
     *
     * \return The number of cards dealt to this hand
     */
    int getNumberOfCards() const;

    /** \brief Determine if the hand is proven to be out of given suit
     *
     * \param suit suit to query
     *
     * \return true if the hand is known to be out of given suit, false if the
     * the hand is known to not be out of the suit, indeterminate otherwise
     */
    boost::logic::tribool isOutOfSuit(Suit suit) const;

    /** \brief Get iterator to the beginning of cards
     *
     * \sa handCardIterator()
     */
    auto begin() const;

    /** \brief Get iterator to the end of cards
     *
     * \sa handCardIterator()
     */
    auto end() const;

private:

    /** \brief Handle for subscribing to reveal state events
     *
     * \sa subscribe()
     */
    virtual void handleSubscribe(
        std::weak_ptr<CardRevealStateObserver> observer) = 0;

    /** \brief Handle for requesting reveal of cards
     *
     * It may be assumed that n < getNumberOfCards() for each n in the range
     *
     * \sa requestReveal()
     */
    virtual void handleRequestReveal(const IndexVector& ns) = 0;

    /** \brief Handle for marking a card as played
     *
     * It may be assumed that n < getNumberOfCards().
     *
     * \sa markPlayed()
     */
    virtual void handleMarkPlayed(int n) = 0;

    /** \brief Handle for returning a card
     *
     * It may be assumed that n < getNumberOfCards() and isPlayed(n) == false.
     *
     * \sa getCard()
     */
    virtual const Card& handleGetCard(int n) const = 0;

    /** \brief Handle for returning whether a card has been played
     *
     * It may be assumed that n < getNumberOfCards().
     *
     * \sa isPlayed()
     */
    virtual bool handleIsPlayed(int n) const = 0;

    /** \brief Handle for returning the number of cards dealt to the hand
     *
     * \sa getNumberOfCards()
     */
    virtual int handleGetNumberOfCards() const = 0;

    /** \brief Handle for determining if the hand can be proven to be out of
     * suit
     *
     * The derived class may choose to override this method if it can know
     * from any other source than the known distribution of the cards (such as
     * trusted server, zero‐knowledge proof etc.) that the player is out of
     * given suit.
     *
     * \note The implementation of this method in a derived class does not
     * need to study known cards in order to determine if a player is out of
     * suit. isOutOfSuit() will do that if this method returns indeterminate.
     *
     * The default implementation always returns indeterminate.
     *
     * \sa isOutOfSuit()
     */
    virtual boost::logic::tribool handleIsOutOfSuit(Suit suit) const;
};

template<typename IndexIterator>
void Hand::requestReveal(IndexIterator first, IndexIterator last)
{
    const auto n = getNumberOfCards();
    const auto fail = std::any_of(
        first, last, [n](const auto m) { return m >= n; });
    if (fail) {
        throw std::out_of_range {"Card index out of range"};
    }
    handleRequestReveal(IndexVector(first, last));
}


/** \brief Create iterator for iterating over cards in a hand
 *
 * \param hand the hand from which the cards are retrieved
 * \param n the index where iterating the cards begins
 *
 * \return Iterator that, when dereferenced, returns constant reference to nth
 * card from the hand. This iterator, unlike Hand::getCard(), skips played
 * cards.
 */
inline auto handCardIterator(const Hand& hand, int n)
{
    const auto make_iterator = [&hand](const int m)
    {
        return boost::make_transform_iterator(
            boost::make_counting_iterator(m),
            [&hand](const int i) { return hand.getCard(i); });
    };
    return boost::make_indirect_iterator(
        boost::make_filter_iterator(
            [](const auto& card) { return card; },
            make_iterator(n),
            make_iterator(hand.getNumberOfCards())));
}

inline auto Hand::begin() const
{
    return handCardIterator(*this, 0);
}

inline auto Hand::end() const
{
    return handCardIterator(*this, getNumberOfCards());
}

/** \brief Find the card with given type from the given hand
 *
 * \param hand the hand to search from
 * \param cardType the card type to search for
 *
 * \return index of the first occurence of a known card with given type, or
 * none if one couldn’t be found
 */
std::optional<int> findFromHand(
    const Hand& hand, const CardType& cardType);

/** \brief Determine if the given card can be played from the given hand
 *
 * This function can be determined if a \c hand.markPlayed(n) call immediately
 * following this call would succeed. Unlike the methods in the Hand interface,
 * this function is safe to call with an “untrusted” argument \p n, because the
 * function does not throw even if \p n is out of range.
 *
 * \return true if \p n is a valid index (0 <= \p n < \c
 * hand.getNumberOfCards()) of a card that hasn’t been played from the \p hand
 * yet, false otherwise
 */
bool canBePlayedFromHand(const Hand& hand, int n);

/** \brief Convenience function for requesting reveal of all cards in the hand
 *
 * This function simply calls hand.requestReveal() with range containing all
 * cards in the hand.
 *
 * \p hand the hand for which the request is revealed
 */
void requestRevealHand(Hand& hand);

}

#endif // HAND_HH_
