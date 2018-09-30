/** \file
 *
 * \brief Definition of Bridge::Main::BridgeGameInfo interface
 */

#ifndef MAIN_BRIDGEGAMEINFO_HH_
#define MAIN_BRIDGEGAMEINFO_HH_

namespace Bridge {

namespace Engine {

class BridgeEngine;
class DuplicateGameManager;

}

namespace Main {

/** \brief Provide read‐only access to a bridge game
 */
class BridgeGameInfo {
public:

    virtual ~BridgeGameInfo();

    /** \brief Get bridge engine for the game
     *
     * \return Reference to Engine::BridgeEngine object containing the state of
     * the current deal. The reference is invalidated when the lifetime of
     * BridgeGameInfo object ends.
     */
    const Engine::BridgeEngine& getEngine() const;

    /** \brief Get game manager for the game
     *
     * \return Reference to Engine::GameManager object containing the state of
     * the game. The reference is invalidated when the lifetime of
     * BridgeGameInfo object ends.
     *
     * \todo Ideally this would return more generic GameManager object and
     * different score formats would be handled indirectly.
     */
    const Engine::DuplicateGameManager& getGameManager() const;

private:

    /** \brief Handle for returning reference to Engine::BridgeEngine
     *
     * \sa getEngine()
     */
    virtual const Engine::BridgeEngine& handleGetEngine() const = 0;

    /** \brief Handle for returning reference to Engine::GameManager
     *
     * \sa getGameManager()
     */
    virtual const Engine::DuplicateGameManager&
    handleGetGameManager() const = 0;
};

}
}

#endif // MAIN_BRIDGEGAMEINFO_HH_
