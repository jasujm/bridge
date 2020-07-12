/** \file
 *
 * \brief Definition of Bridge::Deal interface
 */

#ifndef DEAL_HH_
#define DEAL_HH_

#include "bridge/Uuid.hh"

#include <functional>
#include <optional>
#include <utility>

#include <boost/core/noncopyable.hpp>

namespace Bridge {

class Bidding;
class Hand;
struct Position;
class Trick;
struct Vulnerability;

/** \brief Phase of a bridge deal
 *
 * \sa Deal
 */
enum class DealPhase {
    BIDDING,
    PLAYING,
    ENDED,
};

/** \brief A record of a bridge deal
 */
class Deal : boost::noncopyable {
public:

    /** \brief Pair of trick and position of the winner
     *
     * \sa getTrick()
     */
    using TrickPositionPair = std::pair<
        std::reference_wrapper<const Trick>,
        std::optional<Position>>;

    virtual ~Deal();

    /** \brief Get the UUID of the deal
     */
    const Uuid& getUuid() const;

    /** \brief Get the deal phase
     */
    DealPhase getPhase() const;

    /** \brief Get the vulnerability of the deal
     */
    Vulnerability getVulnerability() const;

    /** \brief Get the position currently in turn
     *
     * \return Position of the player who is next to act. During the
     * playing phase declarer takes turns instead of dummy. If the
     * deal has ended, return none.
     */
    std::optional<Position> getPositionInTurn() const;

    /** \brief Get the hand that currently has turn
     *
     * \return The hand that the next card is played from. If the deal
     * is not in the playing phase, return nullptr.
     */
    const Hand* getHandInTurn() const;

    /** \brief Get the hand of the player seated at the given position
     *
     * \param position the position
     *
     * \return the hand of the player at the given position
     */
    const Hand& getHand(Position position) const;

    /** \brief Get the position of given hand
     *
     * \param hand the hand
     *
     * \return the position corresponding to the hand, or none if the
     * hand does not participate in the deal
     */
    std::optional<Position> getPosition(const Hand& hand) const;

    /** \brief Determine whether a hand is publicly visible
     *
     * \param position the position
     *
     * \return True if the hand as \p position is visible to all,
     * false otherwise. A hand is visible if it belongs to the dummy
     * and the opening lead has been played, or if the deal has ended.
     */
    bool isVisibleToAll(Position position) const;

    /** \brief Get the bidding of the current deal
     */
    const Bidding& getBidding() const;

    /** \brief Get the number of tricks in the deal
     */
    int getNumberOfTricks() const;

    /** \brief Get a tricks in the deal
     *
     * \return A pair containing a trick and the winner of that trick
     * (in case the trick is completed), respectively
     *
     * \throw std::out_of_range if n >= getNumberOfTricks()
     */
    TrickPositionPair getTrick(int n) const;

    /** \brief Retrieve the current trick
     *
     * \return pointer to the current trick, or nullptr if the deal is
     * not in the playing phase
     */
    const Trick* getCurrentTrick() const;

private:

    const Hand& internalGetHandInTurn() const;
    const Position internalGetDeclarerPosition() const;

    /** \brief Handle getting the deal phase
     *
     * \sa getPhase()
     */
    virtual DealPhase handleGetPhase() const = 0;

    /** \brief Handle getting the UUID of the deal
     *
     * \sa getUuid()
     */
    virtual const Uuid& handleGetUuid() const = 0;

    /** \brief Handle getting the vulnerability of the deal
     *
     * \sa getVulnerability()
     */
    virtual Vulnerability handleGetVulnerability() const = 0;

    /** \brief Handle getting the hand of the player seated at the given position
     *
     * \sa getHand()
     */
    virtual const Hand& handleGetHand(Position position) const = 0;

    /** \brief Handle getting the bidding of the current deal
     *
     * During the bidding phase it is assumed that the bidding is not
     * completed. During the playing phase it is assumed that the
     * bidding is completed with a contract.
     *
     * \sa getBidding()
     */
    virtual const Bidding& handleGetBidding() const = 0;

    /** \brief Handle getting the tricks in the current deal
     *
     * During the playing phase it is assumed that getNumberOfTricks() > 0, and
     * that the latest trick is not completed
     *
     * \sa getNumberOfTricks()
     */
    virtual int handleGetNumberOfTricks() const = 0;

    /** \brief Handle getting the tricks in the current deal
     *
     * It may be assumed that 0 < n < getNumberOfTricks().
     *
     * \sa getTrick()
     */
    virtual TrickPositionPair handleGetTrick(int n) const = 0;
};

}

#endif // DEAL_HH_
