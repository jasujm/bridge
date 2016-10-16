/** \file
 *
 * \brief Definition of \ref bridgeprotocol commands
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
 * \b Example. A notification about card being played would consist of the
 * following frames:
 *
 * | N | Content                                    | Notes
 * |---|--------------------------------------------|-----------------------
 * | 1 | play                                       | Event type
 * | 2 | position                                   | Argument key
 * | 3 | "north"                                    | Argument value (JSON)
 * | 4 | card                                       | Argument key
 * | 5 | {"rank":"ace","suit":"spades"}             | Argument value (JSON)
 *
 * \section bridgeprotocolcontrolcommands Control commands
 *
 * \subsection bridgeprotocolcontrolbridgerp bridgerp
 *
 * - \b Command: bridgerp
 * - \b Parameters:
 *   - \e positions: an array consisting of positions
 *      i.e. "north", "east", "south", "west"
 *   - \e cardServer: card server base endpoint (optional)
 * - \b Reply: \e none
 *
 * Each peer MUST send the bridgerp command to each other peer before sending
 * anything else. The command consists of the list of positions the peer
 * intends to control. If card server is intended to be used as card exchange
 * protocol, the cardServer parameter MUST contain the base endpoint for the
 * card server used by the peers.
 *
 * A successful reply means that the peer is accepted and is allowed to
 * control the positions it announces. If cardServer parameter is present, it
 * also means that the peer will establish connection to the card server.
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
 * \subsection bridgeprotocolcontrolget get
 *
 * - \b Command: get
 * - \b Parameters:
 *   - \e keys: array of keys for the values to be retrieved
 * - \b Reply:
 *   - \e positionInTurn: the position of the player who is in turn to act
 *   - \e allowedCalls: array of allowed \e calls to make, if any, \ref jsoncall
 *   - \e calls: array of \e calls that have been made in the current deal
 *   - \e declarer: the \e position of the declarer
 *   - \e contract: the \e contract reached, see \ref jsoncontract
 *   - \e allowedCards: array of allowed \e cards to play, if any,
 *     \ref jsoncardtype
 *   - \e cards: object containing \e cards that are visible to the player
 *   - \e currentTrick: object containing \e cards in the current trick
 *   - \e vulnerability: \e vulnerabilities of the current deal,
 *     see \ref jsonvulnerability
 *   - \e score: the scoresheet, see \ref jsonduplicatescoresheet
 *
 * Get commands returns one or multiple values corresponding to the keys
 * specified. The keys parameter is a array of keys to be retrieved. The reply
 * MUST contain the values corresponding to the keys. Keys that are not
 * recognized SHOULD be igrored.
 *
 * The intention is that this command is used by the clients to query the state
 * of the game. All peers are expected to track the state of the game
 * themselves, and in fact it is not possible for a peer to know what a player
 * controlled by another peer is supposed to see (in particular their
 * cards). Therefore, the result of this command is unspecified if it comes from
 * another peer. In particular, get command from peer MAY be ignored with
 * failure message. In any case any information about hidden cards of a player
 * MUST NOT be revealed to peer or client not controlling that player.
 *
 * The \b positionInTurn parameter contains the position of the player that has
 * the turn to act next. If no player has turn (e.g. because deal has ended and
 * the cards are not dealt yet for the next deal), the position is null. The
 * declarer plays for dummy, so if the next card will be played from the hand of
 * the dummy, the declarer has turn.
 *
 * If the deal is in the bidding phase and the player has turn, the \b
 * allowedCalls parameter is the set of calls allowed to be made by the player.
 *
 * The \b calls parameter is an array of positon–call objects, each describing a
 * position (field “position”) and a \ref jsoncall (field “call”) made by the
 * player at the position. The list is ordered is the order the calls were
 * made. The array is empty if no calls have been made (possibly because cards
 * have not been dealt and bidding not started).
 *
 * An example of an object corresponding to the player at north bidding
 * one clubs would be:
 *
 * \code{.json}
 * {
 *     "position": "north",
 *     "call": { "type": "bid", "bid": { "level": 1, "strain": "clubs" } }
 * }
 * \endcode
 *
 * The \b declarer and \b contract parameters are the position of the declarer
 * and the contract reached in a completed bidding, respectively. If contract
 * has not been reached in the current deal (or perhaps because the deal is not
 * ongoing), both declarer and contract are null values.
 *
 * If the deal is in the playing phase and the player has turn, the \b
 * allowedCards parameter is the set of cards allowed to be played by the
 * player.
 *
 * The \b cards parameter is an object containing mapping from positions to list
 * of cards held by the player in that position. The mapping MUST only contain
 * those hands that are visible to the player, i.e. his own hand and dummy if it
 * has been revealed. The object is empty if cards have not been dealt yet.
 *
 * The \b currentTrick parameter is an object containing mapping from positions
 * to the cards the corresponding player has played to the current trick. The
 * object is empty if there are currently no cards played in a trick (perhaps
 * because the playing phase has not begun or no card has been lead to the
 * current trick).
 *
 * The \b vulnerability parameter is an object containing the current
 * vulnerabilities of the partnerships. If they are not known (perhaps because
 * there is no ongoing deal), the value is empty.
 *
 * The \b score parameter is the current scoresheet of the game.
 *
 * \subsection bridgeprotocolcontroldeal deal
 *
 * - \b Command: deal
 * - \b Parameters:
 *   - cards: an array consisting of cards, see \ref jsoncardtype
 * - \b Reply: \e none
 *
 * If the simple card exchange protocol is used, the leader, i.e. the peer
 * controlling the north position, MUST send this command to all the other
 * peers at the beginning of each deal to announce the cards dealt to the
 * players. The parameter MUST consist of a random permutation of the 52
 * playing cards. Cards in the first 13 positions are dealt to north, the
 * following 13 cards to east, the following 13 cards to south and the last 13
 * cards to west. In each hand, the order of the cards in the list establishes
 * the index order for the purpose of the \ref bridgeprotocolcontrolplay
 * command.
 *
 * Peers MUST ignore the command and reply failure unless it comes from the
 * leader at the beginning of the deal, or if another protocol (such as card
 * server) is used for card exchange.
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
 *   - \e card: see \ref jsoncardtype (alternative)
 *   - \e index: integer
 * - \b Reply: \e none
 *
 * Peers and clients use this command to announce that they want to play the
 * specified card during the playing phase. The first argument is the position
 * playing and the second argument is reference to the card played. The card
 * can be identified either by card type or index. If index (an integer
 * between 0–12) is used, it refers to the index established during the
 * shuffling phase. The index of each card remains the same throughout the
 * whole deal and does not change when cards are played.
 *
 * If the play comes from a client, the peer MUST send the command to other
 * peers. The index variant of referring to the card MUST be used if the card
 * is unknown to the peer. Peers MUST ignore the command and reply failure
 * unless the peer or client controls the player in the position (or if
 * position is the dummy, controls the declarer), the player in the position
 * has turn and it is legal to play the card.
 *
 * The position argument is optional. If omitted, the position of the unique
 * player controlled by the client or the peer is implied. If the peer
 * controls several players, the command MUST fail.
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
 * - \b Parameters:
 *   - \e position: the position of the player to make the call
 *   - \e call: see \ref jsoncall
 *
 * This event is published whenever a call is made.
 *
 * \subsection bridgeprotocoleventbidding bidding
 *
 * - \b Command: bidding
 * - \b Parameters: \e none
 *
 * This event is published whenever contract is reached in bidding
 *
 * \subsection bridgeprotocoleventplay play
 *
 * - \b Command: play
 * - \b Parameters:
 *   - \e position: the position of the hand the card is played from
 *   - \e card: see \ref jsoncardtype
 *
 * This event is published whenever a card is played.
 *
 * \subsection bridgeprotocoleventdummy dummy
 *
 * - \b Command: dummy
 * - \b Parameters: \e none
 *
 * This event is published whenever the hand of the dummy is revealed.
 *
 * \subsection bridgeprotocoleventdealend dealend
 *
 * - \b Command: dealend
 * - \b Parameters: \e none
 *
 * This event is published whenever a deal ends.
 */

#ifndef MAIN_COMMANDS_HH_
#define MAIN_COMMANDS_HH_

#include <string>

namespace Bridge {
namespace Main {

/** \brief See \ref bridgeprotocolcontrolbridgehlo
 */
extern const std::string HELLO_COMMAND;

/** \brief See \ref bridgeprotocolcontrolbridgehlo
 */
extern const std::string POSITION_COMMAND;

/** \brief See \ref bridgeprotocolcontrolbridgerp
 */
extern const std::string PEER_COMMAND;

/** \brief See \ref bridgeprotocolcontrolbridgerp
 */
extern const std::string POSITIONS_COMMAND;

/** \brief See \ref bridgeprotocolcontrolbridgerp
 */
extern const std::string CARD_SERVER_COMMAND;

/** \brief See \ref bridgeprotocolcontrolget
 */
extern const std::string GET_COMMAND;

/** \brief See \ref bridgeprotocolcontrolget
 */
extern const std::string KEYS_COMMAND;

/** \brief See \ref bridgeprotocolcontrolget
 */
extern const std::string POSITION_IN_TURN_COMMAND;

/** \brief See \ref bridgeprotocolcontrolget
 */
extern const std::string ALLOWED_CALLS_COMMAND;

/** \brief See \ref bridgeprotocolcontrolget
 */
extern const std::string CALLS_COMMAND;

/** \brief See \ref bridgeprotocolcontrolget
 */
extern const std::string DECLARER_COMMAND;

/** \brief See \ref bridgeprotocolcontrolget
 */
extern const std::string CONTRACT_COMMAND;

/** \brief See \ref bridgeprotocolcontrolget
 */
extern const std::string ALLOWED_CARDS_COMMAND;

/** \brief See \ref bridgeprotocolcontrolget
 */
extern const std::string CURRENT_TRICK_COMMAND;

/** \brief See \ref bridgeprotocolcontrolget
 */
extern const std::string VULNERABILITY_COMMAND;

/** \brief See \ref bridgeprotocolcontrolget
 */
extern const std::string SCORE_COMMAND;

/** \brief See \ref bridgeprotocolcontrolcall
 */
extern const std::string CALL_COMMAND;

/** \brief See \ref bridgeprotocolcontrolplay
 */
extern const std::string PLAY_COMMAND;

/** \brief See \ref bridgeprotocolcontrolplay
 */
extern const std::string CARD_COMMAND;

/** \brief See \ref bridgeprotocolcontrolplay
 */
extern const std::string INDEX_COMMAND;

/** \brief See \ref bridgeprotocolcontroldeal
 */
extern const std::string DEAL_COMMAND;

/** \brief See \ref bridgeprotocolcontroldeal
 */
extern const std::string CARDS_COMMAND;

/** \brief See \ref bridgeprotocoleventbidding
 */
extern const std::string BIDDING_COMMAND;

/** \brief See \ref bridgeprotocoleventdummy
 */
extern const std::string DUMMY_COMMAND;

/** \brief See \ref bridgeprotocoleventdealend
 */
extern const std::string DEAL_END_COMMAND;

}
}

#endif // CARDSERVER_COMMANDS_HH_
