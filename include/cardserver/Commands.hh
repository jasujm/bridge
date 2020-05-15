/** \file
 *
 * \brief Definition of the Bridge::CardServer commands
 *
 * \page cardserverprotocol Card server protocol
 *
 * This document describes the card server protocol.
 *
 * The key words “MUST”, “MUST NOT”, “REQUIRED”, “SHALL”, “SHALL NOT”,
 * “SHOULD”, “SHOULD NOT”, “RECOMMENDED”, “MAY”, and “OPTIONAL” in
 * this document are to be interpreted as described in RFC 2119
 * (http://tools.ietf.org/html/rfc2119).
 *
 * \section cardserverintro Introduction
 *
 * The card server is a program providing card exchange for a bridge
 * peer defined in the \ref bridgeprotocol, version 0.1. A card server
 * instance is responsible for executing a secure mental card game
 * protocol with other card server instances (card server peer) on
 * behalf of a bridge application (bridge peer). One group of
 * interconnected card servers are needed for each bridge game.
 *
 * When started, the card server opens a control socket (ROUTER) for a
 * bridge application to connect to.
 *
 * The card server opens peer server socket (ROUTER) to receive
 * messages from other card server instances. The card server does not
 * do peer discovery itself, but receives the endpoints from the
 * bridge application during initialization.
 *
 * The card server instance initiates the cryptographic protocols with
 * its card server peers based on the commands received from the
 * bridge application.
 *
 * \warning The card server assumes that the peers are executing the
 * interactive proofs etc. in a certain order. Failure to meet that
 * assumption causes the card server to wait indefinitely for
 * communication from its peer. This protocol does not currently
 * define any commands to abort a protocol sequence.
 *
 * \note The card server protocol is not explicitly versioned. The
 * bridge applications are responsible for negotiating version among
 * themselves. The card server simply receives the endpoints of its
 * peers from the bridge application, and assumes they implement a
 * compatible protocol.
 *
 * \section cardserverlibtmcg LibTMCG
 *
 * The card server protocol is dependent on the way that the LibTMCG
 * library (https://www.nongnu.org/libtmcg/) exchanges messages
 * between the peers. Each card server peer MUST initialize the
 * library in the same way, as specified in this reference
 * implementation.
 *
 * The value of the security level parameter SHOULD be 64, unless the
 * peers have agreed otherwise (outside of this protocol).
 *
 * \section cardserverorder Order
 *
 * A consistent order MUST be established between the card server
 * peers. This is required for the peers to execute the interactive
 * proofs in a correct order. The peer with the lowest order (0) is
 * the leader.
 *
 * The peers controlling the north position has the lowest order, then
 * the peer controlling east, south, and west, respectively. If a peer
 * controls multiple positions, only the lowest order position is
 * considered when determining the overall order of the peers.
 *
 * Example: Peer A controls the player seated in north, peer B the
 * players seated in east and west and peer C the player seated in
 * south. The established order is 0 for A, 1 for B and 2 for C.
 *
 * \section cardservercontrolmessage Control commands
 *
 * The commands sent by the controlling bridge application to the
 * controlled card server use the same structure as the commands
 * between bridge nodes. The bridge application sends a command and
 * the card server replies with a successful or failed reply depending
 * on the outcome.
 *
 * If CurveZMQ is used, the bridge application and the card server
 * MUST use the same keypair. This means that when the bridge
 * application connects to the card server as client, it uses the same
 * key for public key and server key. See
 * http://api.zeromq.org/master:zmq-curve.
 *
 * The structure of the command messages is described in \ref
 * bridgeprotocolcontrolmessage
 *
 * \subsection cardservercontrolinit init
 *
 * - \b Command: init
 * - \b parameters:
 *   - \e order: the order of the card server instance
 *   - \e peers: an array of peer entry objects, see \ref jsonpeerentry
 * - \b Reply: \e none
 *
 * The order parameter MUST be the order of the card server, as
 * determined by the algorithm described in \ref cardserverorder.
 *
 * The peers parameter MUST contain an array of peer entries. There
 * MUST be one entry for each card server peer. Each peer entry object
 * MUST contain the peer endpoint of that card server peer, and MAY
 * contain the CURVE server key used when communicating with that peer
 * if applicable. The order of the peer entries in the array MUST be
 * the same as the order of peers.
 *
 * \note The card server determines the overall order of the peers by
 * inserting a “gap” into the array at the index determined by its own
 * order, and shifting all the remaining peers to the right. Example:
 * Card server A has peers B and C. It receives init command with
 * order parameter 1 and peer entries [B, C]. It then knows that the
 * overall order of the peers is: B, A, C.
 *
 * When receiving this command, the card server MUST examine the
 * entries and connect to each card server peer using the endpoint it
 * learns from the init command. If CurveZMQ is uses, the key in the
 * entry is used as the server key.
 *
 * \b Example: The order of thee card servers is A, B, C. The peer
 * endpoint for card server A is tcp://peerA.example.com:5600. The
 * peer endpoint for card server B is
 * tcp://peerB.example.com:5600. The peer endpoint for card server C
 * is tcp://peerC.example.com:5600. CURVE is not in use. The correct
 * initialization command the bridge peer controlling B is:
 *
 * | Key   | Value
 * |-------|------------------------------------------------------------------
 * | order | 1
 * | peers | [{"endpoint":"tcp://peerA.example.com:5600"},{"endpoint":"tcp://peerC.example.com:5600"}]
 *
 * After all the card server peers have established connection with
 * each other, they generate the necessary groups for the
 * cryptographic protocols.
 *
 * The reply to the init command is successful if it has not been
 * initialized before, and the proofs during the group generation are
 * verified, false otherwise. A successful reply implies that the card
 * server is ready to receive the \ref cardservercontrolshuffle
 * command.
 *
 * \subsection cardservercontrolshuffle shuffle
 *
 * - \b Command: shuffle
 * - \b parameters: \e none
 * - \b Reply: \e none
 *
 * The shuffle command initiates the shuffle protocol between the card
 * server peers. All bridge applications taking part in the game MUST
 * issue shuffle command to their card server synchronously. This
 * command MUST NOT be sent before the init command.
 *
 * The reply is successful if the proofs during the shuffle protocol
 * are verified, false otherwise. A successful reply implies that the
 * card servers have collectively generated a shuffled deck of 52
 * cards and are ready to receive further card manipulation commands.
 *
 * \subsection cardservercontroldraw draw
 *
 * - \b command: draw
 * - \b Parameters:
 *   - \e cards: an array of indices identifying the cards drawn
 * - \b Reply:
 *   - \e cards: an array of 52 cards corresponding to the state of the cards
 *     known to the card server, see \ref jsoncardtype
 *
 * The draw command initiates the draw part of the card drawing protocol.
 *
 * The cards parameter is an array containing the indices of the cards
 * that the card server should draw. The reply is successful if the
 * proofs during the draw protocol are verified, false otherwise. A
 * successful reply is accompanied with an array containing 52
 * (possibly nulled) entries. Unknown cards are represented by nulls.
 *
 * All the other bridge applications taking part in the game MUST
 * issue a \ref cardservercontrolreveal command with the same card
 * indices to their card servers.
 *
 * \note The reply contains card objects from all previous draws since the last
 * shuffle protocol
 *
 * \subsection cardservercontrolreveal reveal
 *
 * - \b command: reveal
 * - \b Parameters:
 *   - \e order: the order parameter of the peer the card is revealed to
 *   - \e cards: an array of indices identifying the cards drawn
 * - \b Reply: \e none
 *
 * The reveal command initiates the reveal part of the card drawing
 * protocol.
 *
 * The order parameter identifies the card server peer drawing the
 * cards. The cards parameter is an array of indices that the peer is
 * drawing. The indices MUST e appear in the same order in the reveal
 * command as they appear in the corresponding draw command.
 *
 * The reply is successful if the proofs during the draw protocol are
 * verified, false otherwise.
 *
 * \subsection cardservercontrolrevealall revealall
 *
 * - \b command: draw
 * - \b Parameters:
 *   - \e cards: an array of indices identifying the cards revealed
 * - \b Reply:
 *   - \e cards: an array of 52 cards corresponding to the state of the cards
 *     known to the card server
 *
 * The revealall command initiates the revealall protocol between the
 * card server peers. All the card server peers MUST be issued a
 * revealall command with the same card indices to initiate the
 * protocol.
 *
 * The revealall protocol combines the draw and reveal protocols in
 * such a way that at the end the specified cards are known to all the
 * players. The parameters and reply are interpreted as in the \ref
 * cardservercontroldraw command, including the requirement that the
 * indices MUST appear in the same order for all peers.
 *
 * \section cardserverpeermessage Peer commands
 *
 * The card server peers communicate with each other by sending
 * messages to peer server sockets. Each message consists of three
 * parts
 *
 * 1. Empty frame
 * 2. The order of the peer that sent the message (one byte unsigned integer)
 * 3. The payload
 *
 * The payload of the messages exchanged between the card server peers
 * are those that the LibTMCG library calls insert to an output
 * stream. A message is sent whenever the stream buffer is
 * synchronized and received whenever it underflows. The protocol
 * between the card server peers is specified by this reference
 * implementation.
 *
 * \section cardservercardexchange Card exchange protocol
 *
 * As described in the previous sections, the bridge applications MUST
 * synchronize the commands to their card servers. This section
 * describes at which points the different parts of the card exchange
 * protocol are executed.
 *
 * \subsection cardserverstartup Startup
 *
 * After the bridge applications have identified that they have
 * established connection with all their peers and agreed to start the
 * game, each bridge peer MUST initialize their card server instance
 * with the \ref cardservercontrolinit command, immediately followed
 * by the first deal sequence.
 *
 * \subsection cardserverdeal Deal
 *
 * Immediately after the startup, or when a new deal is started after
 * the previous one has ended, a deal sequence is initiated. A deal
 * sequence consists of shuffling the deck and revealing each player
 * their cards.
 *
 * At the beginning of the deal sequence, each bridge peer MUST send
 * \ref cardservercontrolshuffle command to their card server
 * instance. This MUST immediately be followed by a sequence of draw
 * and reveal commands determined by the following algorithm:
 *
 * 1. Each bridge peer determines the cards that its peers are allowed to see,
 *    i.e. the indices of the cards that belong to the positions where
 *    the players they control are seated
 *    - North owns cards 0–12
 *    - East owns cards 13–25
 *    - South owns cards 26–38
 *    - West owns cards 39–51
 *
 * 2. Each bridge peer instructs their card server to either draw or
 *    reveal cards in the order established during the initialization.
 *
 * Example: Peer A controls the player seated in north, peer B the
 * players seated in east and west and peer C the player seated in
 * south. The established order is 0 for A, 1 for B and 2 for
 * C. %Bridge application A first draws cards 0–12, then reveals cards
 * 13–25 and 39–51 to B, then reveals cards 26–38 to C. Players B and
 * C match their draws and reveals similarly.
 *
 * \subsection cardserverplay Revealing a played card
 *
 * Whenever a card is played (even if the card was played from the
 * hand of the dummy, thus already known to all players), each peer
 * MUST send the \ref cardservercontrolrevealall command to its card
 * server, identifying the index of the card played.
 *
 * \note The requirement to also execute the reveal sequence for dummy
 * (even though the cards of the dummy are already known to all
 * players) simplifies the implementation of the protocol.
 *
 * \subsection cardserveredummy Revealing the hand of the dummy
 *
 * After the opening lead has been played, and the revealall sequence
 * for that card executed, each peer MUST send the \ref
 * cardservercontrolrevealall command to its card server, identifying
 * the iBridgendices of the cards owned by the dummy.
 */

#ifndef CARDSERVER_COMMANDS_HH_
#define CARDSERVER_COMMANDS_HH_

#include <string>

namespace Bridge {
namespace CardServer {

/** \brief Card server init command
 */
inline constexpr auto INIT_COMMAND = std::string_view {"init"};

/** \brief Order parameter to the card server init command
 */
inline constexpr auto ORDER_COMMAND = std::string_view {"order"};

/** \brief Peers parameter to the card server init command
 */
inline constexpr auto PEERS_COMMAND = std::string_view {"peers"};

/** \brief Card server shuffle command
 */
inline constexpr auto SHUFFLE_COMMAND = std::string_view {"shuffle"};

/** \brief Card server draw command
 */
inline constexpr auto DRAW_COMMAND = std::string_view {"draw"};

/** \brief Cards parameter to several the card server commands
 */
inline constexpr auto CARDS_COMMAND = std::string_view {"cards"};

/** \brief Card server reveal command
 */
inline constexpr auto REVEAL_COMMAND = std::string_view {"reveal"};

/** \brief Card server reveal all command
 */
inline constexpr auto REVEAL_ALL_COMMAND = std::string_view {"revealall"};

}
}

#endif // CARDSERVER_COMMANDS_HH_
