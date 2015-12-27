/** \file
 *
 * \brief Definition of Bridge::Engine::GameManager interface
 */

#ifndef ENGINE_GAMEMANAGER_HH_
#define ENGINE_GAMEMANAGER_HH_

#include <boost/optional/optional_fwd.hpp>

namespace Bridge {

struct Contract;
enum class Partnership;
enum class Position;
enum class Vulnerability;

namespace Engine {

/** \brief The link between the bridge engine and the overall bridge rules
 *
 * GameManager encapsulates the bridge game rules. It provides interface
 * necessary for bridge game logic to inject deal results to the underlying
 * scoring logic and determine if the game has ended. GameManager doesn’t
 * support retrieving the scores as it doesn’t particular type of game
 * (rubber, match points, IMPs, etc.).
 */
class GameManager {
public:

    virtual ~GameManager();

    /** \brief Add result from a deal
     *
     * This method has no effect if the game has ended.
     *
     * \param partnership partnership the contract belongs to
     * \param contract contract in the last deal
     * \param tricksWon number of tricks the partnership won
     */
    void addResult(
        Partnership partnership, const Contract& contract, int tricksWon);

    /** \brief Add passed out deal
     *
     * Indicate that the last deal was passed out. This method has no effect
     * if the game has ended.
     */
    void addPassedOut();

    /** \brief Determine if the game has ended
     *
     * \return true if the game has ended, false otherwise
     */
    bool hasEnded() const;

    /** \brief Determine the opener position
     *
     * \return the position of the opener of the current deal, or none if the
     * game has ended
     */
    boost::optional<Position> getOpenerPosition() const;

    /** \brief Determine the vulnerabilities for the current deal
     *
     * \return the vulnerability status, or none if the game has ended
     */
    boost::optional<Vulnerability> getVulnerability() const;

private:

    /** \brief Handle for adding result from a deal
     *
     * It may be assumed that hasEnded() == false.
     *
     * \sa addResult()
     */
    virtual void handleAddResult(
        Partnership partnership, const Contract& contract, int tricksWon) = 0;

    /** \brief Handle for adding a passed out deal
     *
     * It may be assumed that hasEnded() == false.
     *
     * \sa addPassedOut()
     */
    virtual void handleAddPassedOut() = 0;

    /** \brief Handle for determining if the game has ended
     *
     * \sa hasEnded()
     */
    virtual bool handleHasEnded() const = 0;

    /** \brief Handle for determining the opener position
     *
     * It may be assumed that hasEnded() == false.
     *
     * \sa getOpenerPosition()
     */
    virtual Position handleGetOpenerPosition() const = 0;

    /** \brief Handle for determining the vulnerability status
     *
     * It may be assumed that hasEnded() == false.
     *
     * \sa getVulnerability()
     */
    virtual Vulnerability handleGetVulnerability() const = 0;
};

}
}

#endif // ENGINE_GAMEMANAGER_HH_
