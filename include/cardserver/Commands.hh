/** \file
 *
 * \brief Definition of Bridge::CardServer commands
 *
 * \page cardprotocol Card server protocol
 *
 * \todo Write protocol specification
 */

#ifndef CARDSERVER_COMMANDS_HH_
#define CARDSERVER_COMMANDS_HH_

#include <string>

namespace Bridge {
namespace CardServer {

/** \brief Card server init command
 */
extern const std::string INIT_COMMAND;

/** \brief Order parameter to the card server init command
 */
extern const std::string ORDER_COMMAND;

/** \brief Peers parameter to the card server init command
 */
extern const std::string PEERS_COMMAND;

/** \brief Card server shuffle command
 */
extern const std::string SHUFFLE_COMMAND;

/** \brief Card server draw command
 */
extern const std::string DRAW_COMMAND;

/** \brief Cards parameter to several the card server commands
 */
extern const std::string CARDS_COMMAND;

/** \brief Card server reveal command
 */
extern const std::string REVEAL_COMMAND;

/** \brief Id parameter to the the card server reveal command
 */
extern const std::string ID_COMMAND;

/** \brief Card server reveal all command
 */
extern const std::string REVEAL_ALL_COMMAND;

}
}

#endif // CARDSERVER_COMMANDS_HH_
