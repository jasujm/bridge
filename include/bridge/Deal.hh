/** \file
 *
 * \brief Definition of Bridge::Deal interface
 */

#ifndef DEAL_HH_
#define DEAL_HH_

#include "bridge/Uuid.hh"

#include "enhanced_enum/enhanced_enum.hh"

#include <optional>
#include <string_view>
#include <utility>

#include <boost/core/noncopyable.hpp>

namespace Bridge {

class Bidding;
class Card;
class Hand;
struct Position;
struct Suit;
class Trick;
struct Vulnerability;

/*[[[cog
import cog
import enumecg
import enum
class DealPhase(enum.Enum):
  """%Phase of a bridge deal"""
  BIDDING = "bidding"
  PLAYING = "playing"
  ENDED = "ended"
cog.out(enumecg.generate(DealPhase, primary_type="enhanced", documentation="doxygen"))
]]]*/
/** \brief Label enum for \ref DealPhase
 *
 * This enum was autogenerated for the enhanced enum library. Please see the
 * documentation for more information: https://enhanced-enum.readthedocs.io/
 */
enum class DealPhaseLabel {
    BIDDING,
    PLAYING,
    ENDED,
};

/** \brief %Phase of a bridge deal
 *
 * This enum was autogenerated for the enhanced enum library. Please see the
 * documentation for more information: https://enhanced-enum.readthedocs.io/
 *
 * \sa DealPhaseLabel
 */
struct DealPhase : ::enhanced_enum::enum_base<DealPhase, DealPhaseLabel, std::string_view> {
/// \cond internal
    using ::enhanced_enum::enum_base<DealPhase, DealPhaseLabel, std::string_view>::enum_base;
    static constexpr std::array values {
        value_type { "bidding" },
        value_type { "playing" },
        value_type { "ended" },
    };
/// \endcond
};

/** \brief Promote \ref DealPhaseLabel to \ref DealPhase
 *
 * \param e Label enumerator
 *
 * \return The enhanced enumerator corresponding to \p e
 */
constexpr DealPhase enhance(DealPhaseLabel e) noexcept
{
    return e;
}

/** \brief Associate namespace for \ref DealPhase
 */
namespace DealPhases {
/// \brief Value of enumerator \ref BIDDING
inline constexpr const DealPhase::value_type& BIDDING_VALUE { std::get<0>(DealPhase::values) };
/// \brief Value of enumerator \ref PLAYING
inline constexpr const DealPhase::value_type& PLAYING_VALUE { std::get<1>(DealPhase::values) };
/// \brief Value of enumerator \ref ENDED
inline constexpr const DealPhase::value_type& ENDED_VALUE { std::get<2>(DealPhase::values) };
/// \brief Enumerator of \ref DealPhase
inline constexpr DealPhase BIDDING { DealPhaseLabel::BIDDING };
/// \brief Enumerator of \ref DealPhase
inline constexpr DealPhase PLAYING { DealPhaseLabel::PLAYING };
/// \brief Enumerator of \ref DealPhase
inline constexpr DealPhase ENDED { DealPhaseLabel::ENDED };
}
//[[[end]]]

/** \brief A record of a bridge deal
 */
class Deal : boost::noncopyable {
public:

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

    /** \brief Get the card at index \p n
     *
     * This method allows introspection of cards by index regardless if they are
     * at a player’s hand or already played to a trick. The order of the cards
     * are the same as the order they were dealt by the underlying card manager
     * object.
     *
     * \param n the index of the card
     *
     * \return the card at \p n
     *
     * \throw std::out_of_range unless 0 <= n < \ref N_CARDS
     */
    const Card& getCard(int n) const;

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
     * \throw std::out_of_range if n >= getNumberOfTricks()
     */
    const Trick& getTrick(int n) const;

    /** \brief Get the winner of a trick
     *
     * \param n index of the trick
     *
     * \return The position of the winner of the trick, or nullopt if
     * the trick is not completed
     *
     * \throw std::out_of_range if n >= getNumberOfTricks()
     */
    std::optional<Position> getWinnerOfTrick(int n) const;

    /** \brief Retrieve the current trick
     *
     * \return pointer to the current trick, or nullptr if the deal is
     * not in the playing phase
     */
    const Trick* getCurrentTrick() const;

    /** \brief Get the number of tricks won by the declarer
     *
     * \return The number of tricks won by the declarer, or nullopt if the deal
     * is not in the playing phase.
     */
    std::optional<int> getTricksWonByDeclarer() const;

private:

    const Hand& internalGetHandInTurn() const;
    std::optional<Suit> internalGetTrump() const;
    Position internalGetDeclarerPosition() const;

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

    /** \brief Handle getting the card
     *
     * \sa getHand()
     */
    virtual const Card& handleGetCard(int n) const = 0;

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
    virtual const Trick& handleGetTrick(int n) const = 0;
};

}

#endif // DEAL_HH_
