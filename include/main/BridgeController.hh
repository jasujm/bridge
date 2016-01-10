/** \file
 *
 * \brief Definition of Bridge::BridgeController interface
 */

#ifndef MAIN_BRIDGECONTROLLER_HH_
#define MAIN_BRIDGECONTROLLER_HH_

#include "bridge/Call.hh"

namespace Bridge {

struct CardType;

/** \brief Interface for controlling a bridge game
 *
 * BridgeController is an interface for a class used to handle commands from
 * the active player. In a multiplayer bridge game that is the local player,
 * i.e. the player who controls the current bridge game instance. Commands for
 * remote player are expected to come from different sources, i.e. through
 * some other interface, message queue etc.
 */
class BridgeController
{
public:

    virtual ~BridgeController();

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
};

}

#endif // MAIN_BRIDGECONTROLLER_HH_
