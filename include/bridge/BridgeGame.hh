/** \file
 *
 * \brief Definition of Bridge::BridgeGame interface
 */

#ifndef BRIDGEGAME_HH_
#define BRIDGEGAME_HH_

#include "bridge/Call.hh"

namespace Bridge {

struct CardType;
struct GameState;

/** \brief High level interface for controlling a bridge game
 *
 * The commands given using this interface are expected to come from the
 * active player. In a multiplayer bridge game that is the local player,
 * i.e. the player who controls the current bridge game instance. Commands for
 * remote player are expected to come from different sources, i.e. through
 * some other interface, message queue etc.
 */
class BridgeGame
{
public:

    virtual ~BridgeGame();

    /** \brief Make call for the current player
     *
     * \param call the call to be made
     */
    void call(const Call& call);

    /** \brief Play card for the current player
     *
     * \param cardType the type of the card to be played
     */
    void play(const CardType& cardType);

    /** \brief Determine game state
     */
    GameState getState() const;

private:

    /** \brief Handle for making a call
     *
     * \sa call()
     */
    virtual void handleCall(const Call& call) = 0;

    /** \brief Handle for playing a card
     *
     * \sa play()
     */
    virtual void handlePlay(const CardType& cardType) = 0;

    /** \brief Handle for determining the current game state
     *
     * \sa getState()
     */
    virtual GameState handleGetState() const = 0;
};

}

#endif // BRIDGEGAME_HH_
