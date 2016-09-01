/** \file
 *
 * \brief Definition of Bridge::Main::BridgeMain class
 *
 * \page bridgeprotocol Bridge protocol
 *
 * \note The protocol is not stable and evolves as the application is
 * developed.
 *
 * The key words “MUST”, “MUST NOT”, “REQUIRED”, “SHALL”, “SHALL NOT”,
 * “SHOULD”, “SHOULD NOT”, “RECOMMENDED”, “MAY”, and “OPTIONAL” in this
 * document are to be interpreted as described in RFC 2119
 * (http://tools.ietf.org/html/rfc2119).
 *
 * \section bridgeprotocolintro Introduction
 *
 * Two kinds of entities can connect to the control socket: peers and
 * clients. Peers are equal backends, each keeping its own state of the game,
 * with peers exchanging only necessary information, such as calls and cards
 * played, to make the game possible. Clients are frontends that receive
 * processed state from one peer and relying on the peer to convey commands
 * for the player they control. A program implementing the client protocol is
 * typically the one the user interacts with.
 *
 * Bridge protocol uses ZMTP over TCP (http://rfc.zeromq.org/spec:23). Each
 * peer MUST open control socket (ROUTER) for communicating commands and game
 * state with clients, and SHOULD open event socket (PUB) for publishing
 * events. Clients and other peers wishing to interact with the peer MUST
 * connect to the control socket using a ZMTP socket compatible with
 * ROUTER. In addition they MAY connect to the event socket to subscribe to
 * one or more events.
 *
 * The communication with the control socket is based on requests and
 * replies. The peer or client connecting to the control socket sends a
 * command (possibly with arguments) and receives a reply indicating whether
 * or not the command was successful. The command is a string identifier. The
 * parameters are command specific data structures serialized into JSON. The
 * control socket accepts same commands from both peers and clients, but the
 * way they react to the command differs.
 *
 * Clients typically also connect to the event socket, from which they receive
 * asynchronously information about events coming from other peers and clients.
 *
 * \section bridgeprotocolcontrolmessage Control messages
 *
 * All ZMTP messages sent to the control socket of a peer MUST consist of an
 * empty frame, the command frame and command dependent arguments consisting
 * of alternating key and value frames. The command and argument key parts
 * MUST be a string of ASCII lower case letters a—z. The argument values MUST
 * be JSON data structures encoded in UTF‐8.
 *
 * \b Example. A valid command to play seven of diamonds from the south hand
 * would consist of the following four frames:
 *
 * | N | Content                        | Notes                        |
 * |---|--------------------------------|------------------------------|
 * | 1 |                                | Empty frame                  |
 * | 2 | play                           | No quotes (not JSON)         |
 * | 3 | position                       | Key for position argument    |
 * | 4 | "south"                        | Quotes required (valid JSON) |
 * | 5 | card                           | Key for card argument        |
 * | 6 | {"rank":"7","suit":"diamonds"} |                              |
 *
 * Commands with extraneous arguments MUST be accepted. Unrecognized arguments
 * SHOULD be ignored.
 *
 * In the following sections when describing the commands, the empty frame and
 * the serialization are ignored. Links to the pages describing the JSON
 * representation of the parameters are presented instead.
 *
 * The reply to the command MUST consist of an empty frame, status frame,
 * frame identifying the command and command dependent number of alternating
 * key and value frames for parameters accompanying the reply. The status MUST
 * be either success or failure depending on the state of the command. The
 * identification frame MUST be the same as the command frame of the message
 * it replies to. The arguments MUST be JSON data structures encoded in
 * UTF‐8. A failed reply MUST NOT be accompanied by parameters.
 *
 * \b Example. A successful reply to the bridgehlo command consists of the
 * following three frames:
 *
 * | N | Content                        | Notes                        |
 * |---|--------------------------------|------------------------------|
 * | 1 |                                | Empty frame                  |
 * | 2 | success                        |                              |
 * | 3 | bridgehlo                      |                              |
 * | 4 | position                       |                              |
 * | 5 | "north"                        |                              |
 *
 * The peer MUST reply to every command that has valid format, including the
 * number and representation of the arguments.
 *
 * The peer SHOULD reply to messages that are not valid commands, commands it
 * does not recognize or commands from peers and clients it has not
 * accepted. The reply to any of the previous MUST be failure.
 *
 * The control socket MUST NOT send any messages except replies to the
 * commands received.
 *
 * \section bridgeprotocoleventmessage Event messages
 *
 * The messages the peer publishes through the event socket MUST follow the
 * structure of the commands sent to the control socket, except that they MUST
 * NOT be prepended by the empty frame.
 *
 * \b Example. A notification about deal ending would consist of the following
 * frames:
 *
 * | N | Content                                    | Notes
 * |---|--------------------------------------------|-----------------------
 * | 1 | dealend                                    | Event type
 * | 2 | score                                      | Argument key
 * | 3 | [{"partnership":"northSouth","score":420}] | Argument value (JSON)
 *
 * \section bridgeprotocolcontrolcommands Control commands
 *
 * \subsection bridgeprotocolcontrolbridgerp bridgerp
 *
 * - \b Command: bridgerp
 * - \b Parameters:
 *   - \e positions: an array consisting of positions
       i.e. "north", "east", "south", "west"
 * - \b Reply: \e none
 *
 * Each peer MUST send the bridgerp command to each other peer before sending
 * anything else. The command consists of the list of positions the peer
 * intends to control. A successful reply means that the peer is allowed to
 * control the positions it announces.
 *
 * A peer MUST reply failure if the peer or any other peer it has accepted
 * already controls any of the positions.
 *
 * \subsection bridgeprotocolcontrolbridgehlo bridgehlo
 *
 * - \b Command: bridgehlo
 * - \b Parameters: \e none
 * - \b Reply:
 *   - \e position: the position client is allowed to act for
 *
 * Each client MUST send bridgehlo to one of the peers before sending anything
 * else. A successful reply to the command consists of the position the peer
 * assigns to the client. The peer MUST assign one of the positions it
 * controls itself. It MUST assign different position to each client.
 *
 * \subsection bridgeprotocolcontrolstate state
 *
 * - \b Command: state
 * - \b Parameters:
 *   - \e position (optional)
 * - \b Reply:
 *   - \e state: the state of the deal, see \ref jsondealstate
 *   - \e allowedCalls: array of allowed \e calls to make, if any, \ref jsoncall
 *   - \e allowedCards: array of allowed \e cards to play, if any,
 *     \ref jsoncardtype
 *
 * The first reply parameter of this command MUST be the deal state as is
 * known both to the player in the position and the peer itself.
 *
 * If the deal is in the bidding phase and the player in the position has
 * turn, the second reply parameter MUST be the set of calls allowed to be
 * made by the player.
 *
 * If the deal is in the playing phase and the player in the position has
 * turn, the third reply parameter MUST be the set of cards allowed to be
 * played by the player.
 *
 * The position argument is optional. If omitted, the position of the unique
 * player controlled by the client or the peer is implied. If the peer
 * controls several players, the command MUST fail.
 *
 * \subsection bridgeprotocolcontroldeal deal
 *
 * - \b Command: deal
 * - \b Parameters:
 *   - cards: an array consisting of cards, see \ref jsoncardtype
 * - \b Reply: \e none
 *
 * The leader, i.e. the peer controlling the north position, MUST send this
 * command to all the other peers at the beginning of each deal to announce
 * the cards dealt to the players. The parameter MUST consist of a random
 * permutation of the 52 playing cards. Cards in the first 13 positions are
 * dealt to north, the following 13 cards to east, the following 13 cards to
 * south and the last 13 cards to west.
 *
 * Peers MUST ignore the command and reply failure unless it comes from the
 * leader at the beginning of the deal.
 *
 * \subsection bridgeprotocolcontrolcall call
 *
 * - \b Command: call
 * - \b Parameters:
 *   - \e position (optional)
 *   - \e call: see \ref jsoncall
 * - \b Reply: \e none
 *
 * Peers and clients use this command to announce that they want to make the
 * specified call during the bidding. The first argument is the position
 * calling and the second argument is the call.
 *
 * If the call comes from a client, the peer MUST send the command to other
 * peers. Peers MUST ignore the command and reply failure unless the peer or
 * client controls the player in the position, the player in the position has
 * turn and the call is legal.
 *
 * The position argument is optional. If omitted, the position of the unique
 * player controlled by the client or the peer is implied. If the peer
 * controls several players, the command MUST fail.
 *
 * \subsection bridgeprotocolcontrolplay play
 *
 * - \b Command: play
 * - \b Parameters:
 *   - \e position (optional)
 *   - \e card: see \ref jsoncardtype
 * - \b Reply: \e none
 *
 * Peers and clients use this command to announce that they want to play the
 * specified card during the playing phase. The first argument is the position
 * playing and the second argument is the card played.
 *
 * If the call comes from a client, the peer MUST send the command to other
 * peers. Peers MUST ignore the command and reply failure unless the peer or
 * client controls the player in the position (or if position is the dummy,
 * controls the declarer), the player in the position has turn and it is legal
 * to play the card.
 *
 * The position argument is optional. If omitted, the position of the unique
 * player controlled by the client or the peer is implied. If the peer
 * controls several players, the command MUST fail.
 *
 * \subsection bridgeprotocolcontrolscore score
 *
 * - \b Command: score
 * - \b Parameters: \e none
 * - \b Reply:
 *   - \e score: the scoresheet, see \ref jsonduplicatescoresheet
 *
 * The reply to this command MUST be the current scoresheet of the game.
 *
 * \section bridgeprotocoleventcommands Event commands
 *
 * The peer SHOULD publish the following events through the event socket:
 *
 * \subsection bridgeprotocoleventdeal deal
 *
 * - \b Command: deal
 * - \b Parameters: \e none
 *
 * This event is published whenever new cards are dealt.
 *
 * \subsection bridgeprotocoleventcall call
 *
 * - \b Command: call
 * - \b Parameters: \e none
 *
 * This event is published whenever a call is made.
 *
 * \subsection bridgeprotocoleventplay play
 *
 * - \b Command: play
 * - \b Parameters:
 *   - \e position
 *   - \e card: see \ref jsoncardtype
 *
 * This event is published whenever a card is played.
 *
 * \subsection bridgeprotocoleventdealend dealend
 *
 * - \b Command: dealend
 * - \b Parameters:
 *   - \e score: see \ref jsonduplicatescoresheet
 *
 * This event is published whenever a deal ends. The parameter is the
 * scoresheet of the game so far.
 */

#ifndef MAIN_BRIDGEMAIN_HH_
#define MAIN_BRIDGEMAIN_HH_

#include "bridge/Position.hh"

#include <boost/range/any_range.hpp>
#include <zmq.hpp>

#include <deque>
#include <memory>

namespace Bridge {

/** \brief The main configuration and high level logic for the bridge backend
 *
 * The main class BridgeMain is responsible for configuring the application
 * communicating with peers and clients using the \ref bridgeprotocol.
 */
namespace Main {

/** \brief Configure and run the event loop for the Bridge backend
 *
 * When constructed, BridgeMain configures and sets up Bridge peer application
 * and sockets for communicating with clients and other peers. Two sockets are
 * opened into consecutive ports: the control socket and the event socket.
 *
 * The backend starts processing and publishing messages when run() is
 * called. The destructor closes sockets and cleans up the application.
 *
 * \sa \ref bridgeprotocol
 */
class BridgeMain {
public:

    /** \brief Parameter to BridgeMain()
     */
    using PositionRange = boost::any_range<
        Position, boost::forward_traversal_tag>;

    /** \brief Parameter for BridgeMain()
     */
    using EndpointRange = boost::any_range<
        std::string, boost::single_pass_traversal_tag>;

    /** \brief Create Bridge backend
     *
     * \param context the ZeroMQ context for the game
     * \param baseEndpoint the base endpoint for control and event sockets
     * \param positionsControlled the positions controlled by the application
     * \param peerEndpoints the base endpoints of the peers
     *
     * \note The control socket is bound to \p baseEndpoint. The event socket
     * is bound to the same interface but one larger port.
     */
    BridgeMain(
        zmq::context_t& context,
        const std::string& baseEndpoint,
        PositionRange positionsControlled,
        EndpointRange peerEndpoints);

    ~BridgeMain();

    /** \brief Run the backend message queue
     *
     * This method blocks until terminate() is called.
     */
    void run();

    /** \brief Terminate the backend message queue
     *
     * This method is intended to be called from signal handler for clean
     * termination.
     */
    void terminate();

private:

    class Impl;
    std::shared_ptr<Impl> impl;
};

}
}

#endif // MAIN_BRIDGEMAIN_HH_
