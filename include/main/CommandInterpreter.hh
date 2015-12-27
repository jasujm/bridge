/** \file
 *
 * \brief Definition of Bridge::Main::CommandInterpreter class
 */

#ifndef MAIN_COMMANDINTERPRETER_HH_
#define MAIN_COMMANDINTERPRETER_HH_

#include <boost/core/noncopyable.hpp>

#include <string>

namespace Bridge {

class BridgeGame;

namespace Main {

/** \brief A helper for parsing command strings to be forwarded to BridgeGame
 */
class CommandInterpreter : private boost::noncopyable {
public:

    /** \brief  Create command interpreter
     *
     * \note CommandInterpreter borrows reference to the given BridgeGame
     * object. The user of this class in responsible for ensuring that the
     * lifetime of the BridgeGame instance exceeds the lifetime of the command
     * interpreter.
     *
     * \param game the game object the commands are forwarded to
     */
    CommandInterpreter(BridgeGame& game);

    /** \brief Interpret given command
     *
     * A valid command will be interpreted either as call or play. A valid
     * command is forwarded to the game object. An invalid command is not
     * forwarded.
     *
     * \param command the command to interpret
     *
     * \return true if the command was valid, false otherwise
     */
    bool interpret(const std::string& command);

private:

    BridgeGame& game;
};


}
}

#endif // MAIN_COMMANDINTERPRETER_HH_
