/** \file
 *
 * \brief Definition of the Bridge::CardServer commands
 *
 * \page cardserverprotocol Card server protocol
 *
 * \note The protocol is not stable and evolves as the application is
 * developed.
 *
 * \todo %Card server protocol and its description are currently complex. In long
 * term ideally
 * - The protocol description should be clarified (especially the roles between
 *   different kinds of peers: bridge peer controlling the card server and the
 *   card server peers communicating with each other)
 * - The protocol itself should be streamlined (especially by reducing things
 *   that need to be synchronized between peers)
 *
 * The key words “MUST”, “MUST NOT”, “REQUIRED”, “SHALL”, “SHALL NOT”,
 * “SHOULD”, “SHOULD NOT”, “RECOMMENDED”, “MAY”, and “OPTIONAL” in this
 * document are to be interpreted as described in RFC 2119
 * (http://tools.ietf.org/html/rfc2119).
 *
 * \section cardserverintro Introduction
 *
 * The card server is a component providing the card exchange services for a
 * bridge peer defined in the \ref bridgeprotocol. A card server instance is
 * responsible for executing a secure mental card game protocol with card server
 * peers on behalf of its controlling bridge peer. One group of interconnected
 * card servers are needed for each bridge game instance where the card exchange
 * protocol is used.
 *
 * When started, the card server opens a control socket (ROUTER) for a bridge
 * application to connect to.
 *
 * The initialization command contains information about the card server peers,
 * as the card server is unable to do peer discovery by itself. For each card
 * server peer the card server instance opens another PAIR socket used to
 * communicate with that peer.
 *
 * \note The socket pattern (namely opening one PAIR socket for each endpoint
 * the card server intends to communicate with) is not conventional in ZeroMQ
 * application. It is driven mainly by the need to adapt the LibTMCG API (based
 * on interactive proofs being performed through std::iostream objects within
 * single blocking function call reading one stream at a time) to the ZeroMQ
 * API.
 *
 * The card server instance initiates the cryptographic protocols with its card
 * server peers based on the commands received from the control socket. It is
 * important that the bridge peers synchronize their commands in such a way that
 * each command is matched with the peers executing a compatible command, as
 * described in \ref cardservercardexchange.
 *
 * \warning The card server assumes that the peers are executing the interactive
 * proofs etc. in certain order (see the note above) and failure to meet that
 * assumption causes the card server to wait indefinitely for communication from
 * the peer. This protocol does not currently define any commands to abort an
 * already started protocol sequence.
 *
 * \section cardserverorder Order
 *
 * A consistent order MUST be established between the peers. This is required so
 * that the peers execute the interactive proofs in correct order, and to
 * determine which peer in each card server pair listens and which one
 * connects. The peer with the lowest order (0) is the leader.
 *
 * The order is given to the card server by the controlling bridge peer during
 * the initialization. The peers MUST determine the order using the following
 * algorithm:
 *
 * 1. Repeat until order is assigned to each bridge peer (and its card server)
 *    1. Select the peer controlling the player seated in the lowest ordered
 *       position. The positions, in increasing order, are north, east, south
 *       and west.
 *    2. Assign that peer (and its card server) the lowest yet unassigned
 *       order, starting from 0
 *    3. Remove the peer and all positions controlled by the peer from
 *       further considerations
 *
 * Example: Peer A controls the player seated in north, peer B the players
 * seated in east and west and peer C the player seated in south. The
 * established order is 0 for A, 1 for B and 2 for C.
 *
 * For each card server pair the peer that listens and the peer that connects is
 * determined by their relative order. In order to be consistent, the card
 * server with lower order MUST listen to connection and the card server with
 * higher order MUST connect.
 *
 * \section cardservercontrolmessage Control commands
 *
 * The communication between the controlling bridge peer and the controlled card
 * server instance uses the same RPC and serialization scheme as the control
 * messages between bridge nodes, except that the empty frame at the beginning
 * of a message is not used. The bridge peer sends a command and the card server
 * replies with successful or failed reply depending on the outcome.
 *
 * If CurveZMQ is used, the controlling instance MUST set its public key for the
 * connection to the same value as the server key of the card server, and the
 * card server MUST verify the identity for each request.
 *
 * \note The requirement to validate the public key means that the controlling
 * instance and the controlled card server instance MUST share the same keypair.
 *
 * The command protocol is described as in \ref bridgeprotocolcontrolmessage
 *
 * \subsection cardservercontrolinit init
 *
 * - \b Command: init
 * - \b parameters:
 *   - \e order: an integer specifying the order of the card server instance
 *   - \e peers: an array of peer entry objects, see \ref jsonpeerentry
 * - \b Reply: \e none
 *
 * The order parameter MUST be the order determined by the algorithm described
 * in \ref cardserverorder for the card server.
 *
 * The peers parameter MUST contain an array of peer entries. There MUST be one
 * entry for each card server peer. Each peer entry object MUST contain the
 * identity used to refer to that peer in later communication, the base peer
 * endpoint of that peer and CURVE server key used when communicating with that
 * peer (if applicable). The order of the peer entries MUST be the same as the
 * order of peers, except that there is no entry for the controlled card server
 * instance itself.
 *
 * \note The identity is an opaque hex encoded binary object. The controlling
 * peer may use for example the ZeroMQ socket identity of its peers.
 *
 * \note The card server determines the complete order of the peers by inserting
 * a “gap” into the array at the index determined by its own order parameter,
 * and shifting all the remaining peers by one index. Example: Card server A has
 * peers B and C. It receives init command with order parameter 1 and peer
 * entries [B, C]. It then knows that the full order of the peers is: B, A, C.
 *
 * When receiving this command, the card server MUST examine the entries and
 * connect to each card server peer for whom the entry contains base endpoint,
 * and listen to the connection from a peer whose the endpoint is
 * omitted. The rules for choosing the endpoint are the following:
 * - A socket is bound to the same interface as the base endpoint, but with port
 * number increased by the order of the peer
 * - A socket is connected to the same interface as the base endpoint of the
 * peer, but with port number increased by the order of the controlled card
 * server itself. If CurveZMQ is uses, the connecting socket uses the server key
 * in the peer entry.
 *
 * \note The bridge peers is responsible for providing the correct base
 * endpoints to he controlled card server (which it learns from the \ref
 * bridgeprotocolcontrolgame command). The bridge peer is also responsible for
 * omitting the endpoint from the card server entry of the high order peers in
 * order to comply with the connection rules described in \ref cardserverorder.
 *
 * \b Example: The order of thee card servers is A, B, C. The base peer endpoint
 * for card server A is tcp://peerA.example.com:5600. The base peer endpoint for
 * card server B is tcp://peerB.example.com:5600. The base peer endpoint for
 * card server C is irrelevant in this example. The correct initialization
 * command the bridge peer controlling B is:
 *
 * | Key   | Value
 * |-------|------------------------------------------------------------------
 * | order | 1
 * | peers | [{"id":"1234","endpoint":"tcp://peerA.example.com:5600"},{"id":"abcd"}]
 *
 * When receiving this message, card server B connects to
 * tcp://peerA.example.com:5601 to establish connection with card server A, and
 * binds to tcp://peerB.example.com:5602 to establish connection with card
 * server C.
 *
 * After all card servers have established connection with each other, they
 * generate the necessary groups for further cryptographic protocols.
 *
 * The reply to the init command is successful if it has not been initialized
 * before and the proofs during the group generation are verified, false
 * otherwise. A successful reply implies that the card server is ready to
 * receive the \ref cardservercontrolshuffle command.
 *
 * \subsection cardservercontrolshuffle shuffle
 *
 * - \b Command: shuffle
 * - \b parameters: \e none
 * - \b Reply: \e none
 *
 * The shuffle command initiates the shuffle protocol between the card server
 * peers. The bridge peers MUST synchronize the shuffle protocol. This command
 * MUST NOT be sent before the init command.
 *
 * The reply is successful if the proofs during the shuffle protocol are
 * verified, false otherwise. A successful reply implies that the card servers
 * have collectively generated a shuffled deck of 52 cards and are ready to
 * receive further card manipulation commands.
 *
 * \subsection cardservercontroldraw draw
 *
 * - \b command: draw
 * - \b Parameters:
 *   - \e cards: an array of indices identifying the cards drawn
 * - \b Reply:
 *   - \e cards: an array of 52 cards corresponding to the state of the cards
 *     known to the controlled card server, see \ref jsoncardtype
 *
 * The draw command initiates the draw part of the card drawing protocol. The
 * bridge peers MUST synchronize the draw protocol by matching a \ref
 * cardservercontrolreveal command from all the other card server peers to the
 * draw command from the controlled card server.
 *
 * The cards parameter is an array containing the indices of the cards that the
 * controlled card server intends to draw. The reply is successful if the proofs
 * during the draw protocol are verified, false otherwise. A successful reply is
 * accompanied with an array containing 52 (possibly nulled) entries. Card
 * objects correspond to the known cards (those that have been revealed to the
 * card server instance) and nulls correspond to the unknown cards.
 *
 * \note The reply contains card objects from all previous draws since the last
 * shuffle protocol
 *
 * \subsection cardservercontrolreveal reveal
 *
 * - \b command: reveal
 * - \b Parameters:
 *   - \e id: hex encoded string identifying the peer drawing the cards
 *   - \e cards: an array of indices identifying the cards drawn
 * - \b Reply: \e none
 *
 * The reveal command initiates the reveal part of the card drawing
 * protocol. The bridge peers MUST synchronize the draw protocol as described
 * in \ref cardservercontroldraw.
 *
 * The id parameter identifies the card server peer drawing the cards, and MUST
 * be one of the identities provided in the \ref cardservercontrolinit
 * command. The cards parameter is the list of indices that the peer is
 * drawing. The indices MUST be appear in the same order in the reveal as they
 * appear in the corresponding draw command.
 *
 * The reply is successful if the proofs during the draw protocol are verified,
 * false otherwise.
 *
 * \subsection cardservercontrolrevealall revealall
 *
 * - \b command: draw
 * - \b Parameters:
 *   - \e cards: an array of indices identifying the cards revealed
 * - \b Reply:
 *   - \e cards: an array of 52 cards corresponding to the state of the cards
 *     known to the controlled card server
 *
 * The revealall command initiates the revealall protocol between the peers. The
 * bridge peers MUST synchronize the revealall protocol.
 *
 * The revealall protocol combines draw and reveal protocols in such a way that
 * at the end the cards given as parameter to the command are known to all
 * players. The parameters and reply are interpreted as in the \ref
 * cardservercontroldraw command, including the requirement that the indices
 * MUST appear in the same order for all peers.
 *
 * \section cardserverpeermessage Peer commands
 *
 * The messages sent through the PAIR sockets that the card servers use to
 * communicate to each other are the strings that the LibTMCG library calls
 * insert to an output stream. A message is sent whenever the stream buffer is
 * synchronized and received whenever it underflows. The protocol between the
 * card server peers is specified by this reference implementation.
 *
 * \section cardservercardexchange Card exchange protocol
 *
 * As described in the previous sections, the bridge peers MUST synchronize the
 * commands to their card servers. This section describes at which points the
 * different parts of the card exchange protocol are executed. The bridge peers
 * MUST send the commands in the order described in this section in response to
 * the events described.
 *
 * \subsection cardserverstartup Startup
 *
 * After the bridge peers have identified that they have established connection
 * with all peers and agreed to start the game, each bridge peer MUST initialize
 * their respective card server instance with the \ref cardservercontrolinit
 * command, immediately followed by the first deal sequence.
 *
 * \subsection cardserverdeal Deal
 *
 * Immediately after the startup, or when a new deal is started after the
 * previous one has ended, a deal sequence is initiated. A deal sequence
 * consists of shuffling the deck and revealing each player their cards.
 *
 * At the beginning of the deal sequence, each bridge peer MUST send \ref
 * cardservercontrolshuffle command to their controlled card server
 * instance. This MUST immediately be followed by a sequence of draw and reveal
 * commands determined by the following algorithm:
 *
 * 1. Each bridge peer determines the cards that each peer is allowed to see,
 *    i.e. the \e deck indices of the cards that belong to the positions where
 *    the players they control are seated
 *    - North owns cards 0–12
 *    - East owns cards 13–25
 *    - South owns cards 26–38
 *    - West owns cards 39–51
 * 2. For each peer A (including the peer instance itself) in the \ref
 *    cardserverorder parameter order
 *    - If A is the peer instance itself, send \ref cardservercontroldraw
 *      command identifying the \e deck indices of the cards belonging to self
 *    - Otherwise, send the \ref cardservercontrolreveal command identifying A
 *      and the \e deck indices of the cards belonging to A
 *
 * \subsection cardserverplay Revealing a played card
 *
 * When the peer itself or any peer plays a card (even if the card was played
 * from the hand of the dummy), each peer MUST send the \ref
 * cardservercontrolrevealall command to its controlled card server, identifying
 * the \e deck index of the card played. The index is determined by adding the
 * \e hand index of the played card to the \e deck index of the first card owned
 * by the position of the hand the card was played from. See \ref
 * bridgeprotocolcontrolplay for more information.
 *
 * \note The requirement to also execute the reveal sequence for dummy (even
 * though the cards of the dummy are already known to all players) simplifies
 * the implementation of the protocol.
 *
 * \subsection cardserveredummy Revealing the hand of the dummy
 *
 * When the opening lead has been played (and the reveal sequence for that card
 * executed), each peer MUST send the \ref cardservercontrolrevealall command
 * to its controlled card server, identifying the \e deck indices of the cards
 * owned by the dummy.
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
