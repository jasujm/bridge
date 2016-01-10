/** \file
 *
 * \brief Definition of Bridge::Main::BridgeMain class
 */

#ifndef MAIN_BRIDGEMAIN_HH_
#define MAIN_BRIDGEMAIN_HH_

#include <boost/core/noncopyable.hpp>

#include <memory>
#include <string>

namespace Bridge {

class GameState;

namespace Engine {
class DuplicateGameManager;
}

namespace Main {

/** \brief Main business logic of the Bridge application
 *
 * BridgeMain is used to configure and mediate the components of which the
 * main functionality of the bridge application consists of. It can be thought
 * as the main application thread receiving commands from the players, as well
 * as maintaining the state of the game.
 *
 * \todo This class is meant to own the main thread of the application. All
 * input and output should be handled through asynchronous messages, not
 * direct method calls.
 */
class BridgeMain : private boost::noncopyable {
public:

    /** \brief Create bridge main
     *
     * The constructor creates the necessary classes and starts the game.
     */
    BridgeMain();

    ~BridgeMain();

    /** \brief Process command coming from the player
     *
     * \param command the command to process
     *
     * \todo The commands should be input asynchronously. The protocol should
     * be defined.
     */
    void processCommand(const std::string& command);

    /** \brief Determine the state of the game
     *
     * \return GameState object describing the current game state
     */
    GameState getState() const;

    /** \brief Return DuplicateGameManager that manages the game
     *
     * \return Reference to the DuplicateGameManager object owned by this
     * object. The reference stays valid during the lifetime of the BridgeMain
     * object.
     *
     * \todo This is temporary solution required to print scores in UI. Scores
     * should be published by other means.
     */
    const Engine::DuplicateGameManager& getGameManager() const;

private:

    class Impl;
    const std::unique_ptr<Impl> impl;
};

}
}

#endif // MAIN_BRIDGEMAIN_HH_
