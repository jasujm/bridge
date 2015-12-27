/** \file
 *
 * \brief Definition of Bridge::Trick interface
 */

#ifndef TRICK_HH_
#define TRICK_HH_

#include "bridge/BridgeConstants.hh"

#include <boost/optional/optional_fwd.hpp>

#include <cstddef>

namespace Bridge {

class Card;
class Hand;

/** \brief A trick in contract bridge
 */
class Trick {
public:

    /** \brief The numer of cards in a trick
     */
    static constexpr std::size_t N_CARDS_IN_TRICK = N_PLAYERS;

    virtual ~Trick();

    /** \brief Play card to the trick.
     *
     * The play is successful if the card is a known card that belongs to the
     * hand, the hand has turn and it is legal to play the card according to
     * the rules.
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

    /** \brief Determine which hand has the turn
     *
     * \return the hand whose turn it is to play the card, or none if the
     * trick is completed
     */
    boost::optional<const Hand&> getHandInTurn() const;

    /** \brief Determine the winner of the trick
     *
     * \return the hand who has won the trick, or none if the trick is not
     * completed
     */
    boost::optional<const Hand&> getWinner() const;

    /** \brief Retrieve card played by the given hand
     *
     * \return reference to the card played, or none if the hand hasn’t yet
     * played a card
     */
    boost::optional<const Card&> getCard(const Hand& hand) const;

    /** \brief Determine if the trick is completed
     *
     * A trick is completed when each hand has played a card to it
     *
     * \return true if the trick is completed, false otherwise
     */
    bool isCompleted() const;

private:

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

    /** \brief Handle for returning whether the play is allowed
     *
     * This method returns whether the hand in turn can play given card to the
     * trick. It may be assumed that the hand parameter is the current hand
     * and that card.isKnown() == true.
     *
     * \return true if the card may be played, false otherwise
     */
    virtual bool handleIsPlayAllowed(
        const Hand& hand, const Card& card) const = 0;

    /** \brief Handle for returning the winner of the trick
     *
     * It may be assumed isCompleted() == true.
     */
    virtual const Hand& handleGetWinner() const = 0;

    std::size_t internalGetNumberOfCardsPlayed() const;
};

}

#endif // TRICK_HH_
