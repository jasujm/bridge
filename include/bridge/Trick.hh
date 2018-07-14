/** \file
 *
 * \brief Definition of Bridge::Trick interface
 */

#ifndef TRICK_HH_
#define TRICK_HH_

#include "bridge/BridgeConstants.hh"

#include <boost/iterator/counting_iterator.hpp>
#include <boost/iterator/transform_iterator.hpp>

#include <cstddef>
#include <optional>
#include <utility>

namespace Bridge {

class Card;
class Hand;
enum class Suit;

/** \brief A trick in contract bridge
 *
 * A trick is a container of cards played from hands taking part in a bridge
 * game. It enforces turns and bridge trick rules, and can be used to
 * determine the winner of the trick once completed.
 *
 * \todo Currently card not belonging to the suit lead are accepted if the
 * cards in the hand are unknown. This means that there is no rule control or
 * other cheating detection between peers.
 */
class Trick {
public:

    /** \brief The numer of cards in a trick
     */
    static constexpr std::size_t N_CARDS_IN_TRICK = N_PLAYERS;

    virtual ~Trick();

    /** \brief Play card to the trick.
     *
     * The play is successful if and only if canPlay() would return true for
     * the arguments.
     *
     * \note Trick class will borrow the reference to the Card object. The
     * client of the class is responsible of ensuring that the lifetime of the
     * Card objects is longer than the lifetime of the Trick object.
     *
     * \param hand the hand from which card is played to the trick
     * \param card the card played
     *
     * \return true if the card was played successfully, false otherwise
     */
    bool play(const Hand& hand, const Card& card);

    /** \brief Determine if the card can be played from the hand
     *
     * The card can be played if it is a known card, belongs to the hand, the
     * hand has turn and it is legal to play the card according to the
     * rules. The contract bridge rules allow any card to be played to an
     * empty trick and later cards to follow suit if the hand has the lead
     * suit.
     *
     * \param hand the hand from which the card would be played
     * \param card the card to be played
     *
     * \return true if the card can be played, false otherwise
     */
    bool canPlay(const Hand& hand, const Card& card) const;

    /** \brief Determine which hand has the turn
     *
     * \return pointer the hand whose turn it is to play the card, or nullptr
     * if the trick is completed
     */
    const Hand* getHandInTurn() const;

    /** \brief Retrieve card played by the given hand
     *
     * \return pointer to the card played, or nullptr if the hand hasn’t yet
     * played a card
     */
    const Card* getCard(const Hand& hand) const;

    /** \brief Determine if the trick is completed
     *
     * A trick is completed when each hand has played a card to it
     *
     * \return true if the trick is completed, false otherwise
     */
    bool isCompleted() const;

    /** \brief Get iterator to the beginning of the cards in the trick
     *
     * \return Iterator that, when dereferenced, returns a pair containing
     * constant reference to each hand that has played to the trick so far and
     * the corresponding card.
     */
    auto begin() const;

    /** \brief Get iterator to the end of the cards in the trick
     */
    auto end() const;

private:

    auto trickCardIterator(std::size_t n) const;

    /** \brief Handle for adding a card to the trick
     *
     * \param card card to add to the trick
     */
    virtual void handleAddCardToTrick(const Card& card) = 0;

    /** \brief Handle for returning the number cards played to the trick
     *
     * \return The number of card played to the trick so far. It is expected
     * that handleGetNumberOfCardsPlayed() <= \ref N_CARDS_IN_TRICK.
     */
    virtual std::size_t handleGetNumberOfCardsPlayed() const = 0;

    /** \brief Handle for getting the nth card played to the trick
     *
     * It may be assumed that n < handleGetNumberOfCardsPlayed().
     *
     * \param n the index of the card accessed
     *
     * \sa getCard()
     */
    virtual const Card& handleGetCard(std::size_t n) const = 0;

    /** \brief Handle for retrieving the nth hand in turn
     *
     * It may be assumed that N < \ref N_CARDS_IN_TRICK.
     */
    virtual const Hand& handleGetHand(std::size_t n) const = 0;

    std::size_t internalGetNumberOfCardsPlayed() const;
};

/** \brief Determine the winner of the trick
 *
 * The winner of the trick is determined according to the rules of the
 * contract bridge. The hand that played the highest trump, or the highest
 * card of the leading suit if there are no trumps, wins the trick.
 *
 * \param trick the trick
 * \param trump the trump suit, if any
 *
 * \return pointer to the hand who has won the trick, or nullptr if the trick
 * is not completed
 */
const Hand* getWinner(
    const Trick& trick, std::optional<Suit> trump = std::nullopt);

inline auto Trick::trickCardIterator(std::size_t n) const
{
    return boost::make_transform_iterator(
        boost::make_counting_iterator(n),
        [this](const std::size_t n)
        {
            return std::pair<const Hand&, const Card&>(
                handleGetHand(n), handleGetCard(n));
        });
}

inline auto Trick::begin() const
{
    return trickCardIterator(0u);
}

inline auto Trick::end() const
{
    return trickCardIterator(handleGetNumberOfCardsPlayed());
}

}

#endif // TRICK_HH_
