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
 * This document describes network protocol for playing contract bridge. The
 * protocol allows either client‐server or peer‐to‐peer network topologies. It
 * allows several methods of exchanging cards between peers: this document
 * defines a clear text protocol between trusted parties and, together with the
 * \ref cardserverprotocol, a secure mental card game protocol where lower level
 * of trust is required.
 *
 * \section bridgeprotocoltransport Transport
 *
 * Bridge protocol uses ZMTP 3.0 over TCP (http://rfc.zeromq.org/spec:23).
 *
 * \section bridgeprotocolroles Roles
 *
 * There are two kinds of roles: clients and peers. This section describes the
 * requirements clients and peers (collectively known as nodes) must satisfy.
 *
 * \subsection bridgeprotocolpeer Peers
 *
 * - MUST open a control socket (ROUTER) to which nodes connect to
 * - SHOULD open an event socket (PUB) to which other nodes can subscribe for
 *   events
 * - Are REQUIRED to keep record the full state of each game they take part in
 * - MAY represent multiple players in each came they take part in
 * - MAY accept connections from clients
 * - SHOULD delegate the control of the players it represents to the accepted
 *   clients
 * - For each accepted client, MUST handle commands specified in \ref
 *   bridgeprotocolcontrolmessage
 * - MAY accept connections from other peers
 * - MUST agree with all peers taking part in a game about the players each
 *   represents
 * - MUST co‐operate with each other peer that takes part in the same games by
 *   sending commands required in \ref bridgeprotocolcontrolmessage
 * - SHOULD hand over connection to nodes that connect with the same identity as
 *   previously connected node
 *
 * \subsection bridgeprotocolclients Clients
 *
 * - SHOULD connect to one peer (control and event sockets) at a time to control
 *   one of the players it represents
 * - MAY rely on the peer to track the full state of the game
 * - SHOULD set identity to a value that is unique with high probability (such
 *   as binary presentation of UUID it generates)
 *
 * \subsection bridgeprotocolplayers Players
 *
 * In the bridge protocol a \e player is an uniquely identified entity that
 * takes actions in a bridge game subject to the rules of contract bridge. In
 * each game, a peer represents one or more players with the intention of
 * delegating control of the to the clients connecting to the peer. A peer
 * representing a player, or a client controlling a player represented by the
 * peer itself, is said to be allowed to act for that player.
 *
 * The bridge protocol allows multiple network topologies. In pure client‐server
 * model a single peer represents all players and all clients connect to the
 * single peer. In pure peer‐to‐peer model each peer only represents one
 * player. In peer‐to‐peer model the peer can be considered a backend and the
 * client a frontend of a single bridge application.
 *
 * \section bridgeprotocolcontrolmessage Control messages
 *
 * All messages sent to the control socket of a peer MUST consist of an empty
 * frame, the command frame and command dependent arguments consisting of
 * alternating key and value frames. The command and argument key parts MUST
 * consist of printable ASCII characters. The argument values MUST be JSON
 * documents encoded in UTF‐8.
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
 * Commands with additional unspecified arguments MUST be accepted. Unrecognized
 * arguments SHOULD be ignored.
 *
 * \note In the following sections when describing the commands, the empty frame
 * and the serialization are ignored. Links to the pages describing the JSON
 * representation of the parameters are presented instead.
 *
 * The peer MUST reply to properly formatted messages. The reply to the command
 * MUST consist of an empty frame, status frame, frame identifying the command
 * and command dependent number of reply arguments consisting of alternating key
 * and value frames. The status MUST be either success or failure depending on
 * whether or not the command was successfully handled. The identification frame
 * MUST be the same as the command frame of the message it replies to. The
 * arguments MUST be JSON documents encoded in UTF‐8. A failed reply MUST NOT be
 * accompanied by parameters.
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
 * The peer SHOULD reply to messages that are not valid commands, commands it
 * does not recognize or commands from peers and clients it has not
 * accepted. The reply to any of the previous MUST be failure.
 *
 * A peer MUST NOT send any messages through the control socket except replies
 * to the commands received.
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
 * \subsection bridgeprotocolcontrolbridgehlo bridgehlo
 *
 * - \b Command: bridgehlo
 * - \b Parameters:
 *   - \e version: an array of integers describing the version of the protocol
 *   - \e role: a string, either "peer" or "client"
 * - \b Reply: \e none
 *
 * Each node MUST start the communication with this command before sending any
 * other command.
 *
 * The version MUST be an array of integers containing at least one element (the
 * major version). The version MAY contain further elements describing minor,
 * patch etc. versions.
 *
 * The role MUST be either "peer" or "client". The roles are described in \ref
 * bridgeprotocolroles.
 *
 * The peer MAY accept a node if it is compatible with the major version of the
 * protocol. The peer MUST reject the peer if it is not compatible with the
 * version. If rejected, the other peer MAY try to send another bridgehlo
 * command with downgraded version. The development protocol is denoted by major
 * version 0.
 *
 * \subsection bridgeprotocolgame game
 *
 * - \b Command: game
 * - \b Parameters:
 *   - \e positions: an array consisting of positions, see \ref jsonposition
 * - \b Reply: \e none
 *
 * Nodes use this command to join a bridge game. The interpretation of the
 * positions argument depends on the role of the node.
 *
 * If the node is a peer, the argument MUST be present and contain the positions
 * of all the players the node requests to represent. The command MUST fail
 * without effect if at least one of the positions in the list is represented by
 * the peer itself or any other peer it has accepted.
 *
 * If the node is a client, the argument is optional. If present, it MUST
 * contain the preferred positions for the player that the client controls, in
 * descending order of preference. The command MUST fail if none of the
 * positions is available (that is, either not represented by the peer or
 * already controlled by another client). Otherwise the peer SHOULD assign the
 * highest available position in the list for the client to control.
 *
 * The positions argument SHOULD not contain duplicates. A peer MAY either
 * ignore duplicates or fail the command if duplicate values are present.
 *
 * \subsection bridgeprotocolcontrolget get
 *
 * - \b Command: get
 * - \b Parameters:
 *   - \e keys: array of keys for the values to be retrieved
 * - \b Reply:
 *   - \e position: the position of the controlled player
 *   - \e positionInTurn: the position of the player who is in turn to act
 *   - \e allowedCalls: array of allowed \e calls to make, if any, \ref jsoncall
 *   - \e calls: array of \e calls that have been made in the current deal
 *   - \e declarer: the \e position of the declarer
 *   - \e contract: the \e contract reached, see \ref jsoncontract
 *   - \e allowedCards: array of allowed \e cards to play, if any,
 *     \ref jsoncardtype
 *   - \e cards: object containing \e cards that are visible to the player
 *   - \e trick: array containing \e cards in the current trick
 *   - \e tricksWon: see \ref jsontrickswon
 *   - \e vulnerability: \e vulnerabilities of the current deal,
 *     see \ref jsonvulnerability
 *   - \e score: the scoresheet, see \ref jsonduplicatescoresheet
 *
 * Get commands returns one or multiple values corresponding to the keys
 * specified. The keys parameter is a array of keys to be retrieved. The reply
 * MUST contain the values corresponding to the keys. Keys that are not
 * recognized SHOULD be ignored.
 *
 * The intention is that this command is used by the clients to query the state
 * of the game. All peers are expected to track the state of the game
 * themselves, and in fact it is not possible for a peer to know what a player
 * represented by another peer is supposed to see (in particular their
 * cards). Therefore, the result of this command is unspecified if it comes from
 * another peer. In particular, get command from peer MAY be ignored with
 * failure message. In any case any information about hidden cards of a player
 * MUST NOT be revealed to a node not controlling or representing that player.
 *
 * The \b position parameter contains the position of the player that the client
 * controls.
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
 * The \b calls parameter is an array of positon–call pairs, each containing a
 * position (field “position”) and a call (field “call”) made by the player at
 * the position. The array is ordered in the order the calls were made. The
 * array is empty if no calls have been made (possibly because cards have not
 * been dealt and bidding not started).
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
 * The \b trick parameter is an array of position–card pairs, each containing a
 * position (field “position”) and a card (field “card”) played by the position
 * to the trick. The array is ordered in the order that the cards were played to
 * the trick. The array is empty if no cards have been played to the current
 * trick (possibly because the last trick was just completed or the playing
 * phase has not started).
 *
 * The \b tricksWon parameter is an object containing mapping from partnerships
 * to the number of tricks won by each side. If no tricks have been completed
 * (perhaps because bidding is not yet completed), both fields are zero.
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
 * representing the player at the north position, MUST send this command to all
 * the other peers at the beginning of each deal to announce the cards dealt to
 * the players. The parameter MUST consist of a random permutation of the 52
 * playing cards. Cards in the first 13 positions are dealt to north, the
 * following 13 cards to east, the following 13 cards to south and the last 13
 * cards to west. In each hand, the order of the cards in the list establishes
 * the index order for the purpose of the \ref bridgeprotocolcontrolplay
 * command.
 *
 * \note To disambiguate the indexing of the cards determined by their place in
 * the array of dealt cards from other related indexing concepts (most notably
 * then hand index used when playing the cards), the indices of the cards in the
 * dealt cards array are called the \e deck indices.
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
 * All nodes use this command to make the specified call during the bidding.
 *
 * Peers MUST ignore the command and reply failure unless the node is allowed to
 * act for the player in the position, the player has turn and the call is
 * legal. If a successful call command comes from a client, the peer MUST send
 * the command to other peers taking part in the game.
 *
 * The position argument MAY be omitted if the player the node is allowed to act
 * for is unique. The command SHOULD fail if a peer representing multiple
 * players omits the position.
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
 * All nodes use this command to play the specified card during the playing
 * phase. The card can be identified either by card type or index. If index (an
 * integer between 0–12) is used, it refers to the index established during the
 * shuffling phase. The index of each card remains the same throughout the whole
 * deal and does not change when cards are played.
 *
 * \note To disambiguate this index concept from the related concepts of
 * indexing cards (most notably the \e deck index determined during the
 * shuffling), the index used to refer to the place of the card in the hand is
 * called the \e hand index.
 *
 * Peers MUST ignore the command and reply failure unless the node is allowed to
 * act for the player in the position, the player in the position has turn and
 * it is legal to play the card. Declarer plays the hand of the dummy. If the
 * play command comes from a client, the peer MUST send the command to other
 * peers. The index variant of referring to the card MUST be used if the card is
 * unknown to the peer.
 *
 * The position argument MAY be omitted if the player the node is allowed to act
 * for is unique. The command SHOULD fail if a peer representing multiple
 * players omits the position.
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
 * \subsection bridgeprotocoleventtrick trick
 *
 * - \b Command: trick
 * - \b Parameters:
 *   - \e winner: the \e position of the player that wins the trick
 *
 * This event is published whenever a trick is completed.
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
extern const std::string VERSION_COMMAND;

/** \brief See \ref bridgeprotocolcontrolbridgehlo
 */
extern const std::string ROLE_COMMAND;

/** \brief See \ref bridgeprotocolcontrolbridgerp
 */
extern const std::string CARD_SERVER_COMMAND;

/** \brief See \ref bridgeprotocolcontrolgame
 */
extern const std::string GAME_COMMAND;

/** \brief See \ref bridgeprotocolcontrolgame
 */
extern const std::string POSITIONS_COMMAND;

/** \brief See \ref bridgeprotocolcontrolget
 */
extern const std::string GET_COMMAND;

/** \brief See \ref bridgeprotocolcontrolget
 */
extern const std::string POSITION_COMMAND;

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
extern const std::string TRICKS_WON_COMMAND;

/** \brief See \ref bridgeprotocolcontrolget
 */
extern const std::string TRICK_COMMAND;

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

/** \brief See \ref bridgeprotocoleventtrick
 */
extern const std::string TRICK_COMMAND;

/** \brief See \ref bridgeprotocoleventtrick
 */
extern const std::string WINNER_COMMAND;

/** \brief See \ref bridgeprotocoleventdealend
 */
extern const std::string DEAL_END_COMMAND;

}
}

#endif // CARDSERVER_COMMANDS_HH_
