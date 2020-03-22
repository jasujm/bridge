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
 * Bridge protocol uses ZMTP 3.0 over TCP
 * (https://rfc.zeromq.org/spec:23/ZMTP). The traffic is authenticated and
 * encrypted using the curve mechanism
 * (https://rfc.zeromq.org/spec:25/ZMTP-CURVE).
 *
 * \section bridgeprotocolroles Roles
 *
 * There are two kinds of roles: clients and peers. This section describes the
 * requirements clients and peers (collectively known as nodes) must satisfy.
 *
 * \subsection bridgeprotocolpeer Peers
 *
 * - MUST open a control socket (ROUTER) other nodes connect to
 * - MUST open an event socket (PUB) other nodes can subscribe for events
 * - Are REQUIRED to keep record of the full state of each game they take part
 *   in
 * - MAY represent multiple players in each game they take part in
 * - MAY accept connections from clients
 * - SHOULD delegate the control of the players it represents to the accepted
 *   clients
 * - For each accepted client, MUST handle commands specified in \ref
 *   bridgeprotocolcontrolmessage
 * - MAY accept connections from other peers
 * - MUST agree with all peers taking part in a game about the positions each
 *   one controls
 * - MUST co‐operate with the other peers that takes part in the same games by
 *   sending commands required in \ref bridgeprotocolcontrolmessage
 * - SHOULD hand over connection to nodes that connect with the same identity as
 *   previously connected node
 *
 * \subsection bridgeprotocolclients Clients
 *
 * - MUST communicate with a peer using \ref bridgeprotocolcontrolmessage to
 *   control the actions of a player it represents
 * - SHOULD subscribe to the event socket the peer provides
 * - MAY rely on the peer to track the full state of the game
 * - SHOULD set identity to a value that is unique with high probability (such
 *   as binary presentation of an UUID it generates)
 *
 * \subsection bridgeprotocolplayers Players
 *
 * In each game, a peer represents one or more players, possibly with the
 * intention of delegating control of the actions of the player to the clients
 * connecting to the peer. Before the game the peers negotiate which players
 * each one represents. A peer representing a player or a client to whom the
 * control of a player is delegated to is said to be allowed to act for that
 * player.
 *
 * The bridge protocol allows multiple network topologies. In pure client‐server
 * model a single peer represents all players and all clients connect to the
 * single peer. In pure peer‐to‐peer model each peer only represents one
 * player. In peer‐to‐peer model the peer can be considered a backend and the
 * client a frontend of a single bridge application.
 *
 * \section bridgeprotocolbasics Basics
 *
 * The bridge protocol framing are built on top of ZMTP frames.
 *
 * A bridge protocol \b message is a multipart ZMTP message consisting of a
 * prefix consisting of fixed number of frames, followed by a variable number of
 * parameter frames. The prefix depends on the message type, but usually
 * consists at least of a frame identifying the command. The parameter frames
 * consist of key–value pairs, with a key frame followed by a serialized value
 * frame. The command identifiers and key frames SHOULD be printable ASCII
 * strings. The values MUST be UTF‐8 encoded JSON documents.
 *
 * \note Although command identifiers and keys are blobs without further
 * internal structure, restricting to printable ASCII characters eases logging
 * and inspecting the protocol.
 *
 * \note Wherever there is no risk of ambiguity, both ZMTP and bridge messages
 * are just called messages.
 *
 * \section bridgeprotocolcontrolmessage Command messages
 *
 * A \b command is a message whose prefix consists of an empty frame and the
 * command frame. Other peers and clients send control commands to the control
 * socket of the peer in order to control its behavior and the flow of a bridge
 * game.
 *
 * \note Whenever there is no risk of disambiguity, both the whole command
 * message, and the command frame identifying it are called commands.
 *
 * \b Example. A valid command to play seven of diamonds from the south hand
 * would consist of the following eight frames:
 *
 * | N | Content                                | Notes                        |
 * |---|----------------------------------------|------------------------------|
 * | 1 |                                        | Empty frame                  |
 * | 2 | play                                   | No quotes (not JSON)         |
 * | 3 | game                                   | Key for game argument        |
 * | 4 | "9c3fc66b-ab10-4006-8c15-3b4f7eb04846" | Quotes required (valid JSON) |
 * | 5 | player                                 | Key for player argument      |
 * | 6 | "8bc7c6ca-1f19-440c-b5e2-88dc049bca53" |                              |
 * | 7 | card                                   | Key for card argument        |
 * | 8 | {"rank":"7","suit":"diamonds"}         |                              |
 *
 * Commands with additional unspecified arguments MUST be accepted. Unrecognized
 * arguments SHOULD be ignored.
 *
 * \note In the following sections when describing the commands, the empty frame
 * and the serialization are ignored. Links to the pages describing the JSON
 * representation of the parameters are presented instead.
 *
 * \section bridgeprotocolreplymessage Reply messages
 *
 * The peer MUST send a \b reply message to a node sending a control
 * command. However, it MAY omit reply to any command message not consisting of
 * at least the empty frame and the command frame.
 *
 * The prefix of a reply consist of an empty frame, status frame, and
 * a frame echoing back the command. The status frame MUST be a string
 * starting with “OK” for successful reply, or “ERR” for failed
 * reply. The command frame MUST be the same as the command frame of
 * the message it replies to.
 *
 * \b Example. A successful reply to a get command requesting the position of
 * the current player consists of the following five frames:
 *
 * | N | Content  | Notes       |
 * |---|----------|-------------|
 * | 1 |          | Empty frame |
 * | 2 | OK       |             |
 * | 3 | get      |             |
 * | 4 | position |             |
 * | 5 | "north"  |             |
 *
 * \b Example. A failed reply to the get command consists of the following three
 * frames:
 *
 * | N | Content  | Notes       |
 * |---|----------|-------------|
 * | 1 |          | Empty frame |
 * | 2 | ERR      |             |
 * | 3 | get      |             |
 *
 * \note To determine if a reply is successful, an implementation MUST
 * match for the prefix of the string. “OK” and “OK!!!” are equally
 * successful replies. The purpose is to allow new revisions to define
 * more specific status codes by adding suffixes to the base status.
 *
 * The peer SHOULD reply to messages that are not valid commands, commands it
 * does not recognize or commands from peers and clients it has not
 * accepted. The reply to any of the previous MUST be failure. The peer SHOULD
 * NOT reply to messages which do not contain the empty frame and the command
 * frame.
 *
 * A peer MUST NOT send any other messages through the control socket than
 * replies to the commands received.
 *
 * \section bridgeprotocoleventmessage Event messages
 *
 * The peer MUST publish \b event messages trough its event socket when certain
 * events described below take place. The prefix of an event message consists of
 * an event frame. The event frame MUST consist of UUID of the game in the
 * canonical form, a colon, and event type consisting of printable ASCII
 * characters.
 *
 * \note The motivation for having event frame contain the UUID of the game
 * first is to allow the clients to subscribe to events from specific game by
 * using its UUID as prefix.
 *
 * \b Example. A notification about card being played would consist of the
 * following frames:
 *
 * | N | Content                                    | Notes
 * |---|--------------------------------------------|-----------------------
 * | 1 | 9c3fc66b-ab10-4006-8c15-3b4f7eb04846:play  | Event type
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
 *   - \e role: a string, either “peer” or “client”
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
 * \subsection bridgeprotocolcontrolgame game
 *
 * - \b Command: game
 * - \b Parameters:
 *   - \e game (optional for clients)
 *   - \e args: additonal protocol specific arguments (peers only)
 * - \b Reply:
 *   - \e game: the game being set up
 *
 * Both peers and clients use this command to set up a new bridge game.
 *
 * The game argument contains the UUID of the game being set up. A client
 * setting up a new game MAY omit it in which case the peer SHOULD generate UUID
 * for the new game.
 *
 * The args parameter is a JSON object containing additional parameters needed
 * by peers to set up the game. It MUST at least contain key “position”
 * containing array of positions of all the players the node requests to
 * represent. The command MUST fail without effect if at least one of the
 * positions in the list is represented by the peer itself or any other peer it
 * has accepted.
 *
 * If the card server protocol is used, args MUST also contain the card server
 * peer endpoint. If CURVE mechanism is used to secure communication between the
 * card server peers, args MUST also contain the server key of the card server
 * as hex encoded string. For instance, if the card server peer endpoint is
 * tcp://example.com:5555 with CURVE server key, the object would be the
 * following:
 *
 * \code{.json}
 * {
 *     "positions": ["north", "east"],        // positions are example only
 *     "cardServer": {
 *          "endpoint": "tcp://example.com:5555",
 *          "serverKey": "54FCBA24E93249969316FB617C872BB0C1D1FF14800427C594CBFACF1BC2D652"
 *     }
 * }
 * \endcode
 *
 * The peer MAY reject a client or other peer from setting up a game. A possible
 * reason for rejection is an unknown peer trying to take part in a game. If
 * rejected, the command MUST fail without effect.
 *
 * \todo The card exchange protocol is now fixed per instance. It should instead
 * be negotiable on game basis.
 *
 * The positions argument SHOULD not contain duplicates. A peer MAY either
 * ignore duplicates or fail the command if duplicate values are present.
 *
 * \subsection bridgeprotocolcontroljoin join
 *
 * - \b Command: join
 * - \b Parameters:
 *   - \e game: the UUID of the game (optional for clients)
 *   - \e player: the UUID of the player (optional for clients)
 *   - \e position: the preferred position (optional for clients)
 * - \b Reply:
 *   - \e game: the game joined
 *
 * Both clients and peers use this command to join a player in a bridge game.
 *
 * The game argument is the UUID of the game being joined. Clients MAY omit it
 * in which case the peer SHOULD select an available game for the client.
 *
 * A client MAY omit the player argument. If omitted, the peer SHOULD generate
 * UUID for the player controlled by a newly joining node. A client MAY use
 * position argument to announce the preferred position in the game. If not
 * present, the peer MUST choose any free position for the client.
 *
 * Peers MUST provide both player and position arguments. The position MUST be
 * one of the positions the peer has reserved in the previous game command. The
 * player MUST be UUID of the represented player in the position.
 *
 * If a successful join command comes from a client, the peer MUST send the
 * command to other peers taking part in the game.
 *
 * The command MUST fail without an effect if the player is already in the game,
 * the position is occupied by some other player, the command is coming from a
 * peer who has not reserved the position, or the command is coming from a peer
 * requesting a position the peer itself has not reserved.
 *
 * The peer MAY reject a client or other peer from joining the game. If
 * rejected, the command MUST fail without effect.
 *
 * \subsection bridgeprotocolcontrolget get
 *
 * - \b Command: get
 * - \b Parameters:
 *   - \e game: UUID of the game being queried
 *   - \e get: array of keys for the values to be retrieved (optional)
 * - \b Reply:
 *   - \e get: object describing the current state of the game
 *
 * Get commands retrieves one or multiple key–value pairs describing
 * the state of a game. The \e get argument is a array of keys to be
 * retrieved. It MAY be omitted in which case the reply MUST contain
 * the whole state of the game. If included, the reply SHOULD only
 * contain the key–value pairs contained in the array. Keys that are
 * not recognized SHOULD be ignored.
 *
 * The intention is that this command is used by the clients to query
 * the state of the game. All peers are expected to track the state of
 * the game themselves. Therefore, the result of this command is
 * unspecified if it comes from another peer. In particular, get
 * command from peer MAY simply return a failure. In any case any
 * information about hidden cards of a player MUST NOT be revealed to
 * a node not controlling or representing that player.
 *
 * The \e get parameter in the reply is a JSON object consisting of (a
 * subset of) the following key–value pairs:
 *
 * - \e position: the position of the controlled player
 * - \e positionInTurn: the position of the player who is in turn to act
 * - \e allowedCalls: array of allowed \e calls to make, if any, \ref jsoncall
 * - \e calls: array of \e calls that have been made in the current deal
 * - \e declarer: the \e position of the declarer
 * - \e contract: the \e contract reached, see \ref jsoncontract
 * - \e allowedCards: array of allowed \e cards to play, if any,
 *   \ref jsoncardtype
 * - \e cards: object containing \e cards that are visible to the player
 * - \e trick: array containing \e cards in the current trick
 * - \e tricksWon: see \ref jsontrickswon
 * - \e vulnerability: \e vulnerabilities of the current deal,
 *   see \ref jsonvulnerability
 *
 * The \e position parameter contains the position of the player that the client
 * controls.
 *
 * The \e positionInTurn parameter contains the position of the player that has
 * the turn to act next. If no player has turn (e.g. because deal has ended and
 * the cards are not dealt yet for the next deal), the position is null. The
 * declarer plays for dummy, so if the next card will be played from the hand of
 * the dummy, the declarer has turn.
 *
 * If the deal is in the bidding phase and the player has turn, the \e
 * allowedCalls parameter is the set of calls allowed to be made by the player.
 *
 * The \e calls parameter is an array of positon–call pairs, each containing a
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
 * The \e declarer and \e contract parameters are the position of the declarer
 * and the contract reached in a completed bidding, respectively. If contract
 * has not been reached in the current deal (or perhaps because the deal is not
 * ongoing), both declarer and contract are null values.
 *
 * If the deal is in the playing phase and the player has turn, the \e
 * allowedCards parameter is the set of cards allowed to be played by the
 * player.
 *
 * The \e cards parameter is an object containing mapping from positions to list
 * of cards held by the player in that position. The mapping MUST only contain
 * those hands that are visible to the player, i.e. his own hand and dummy if it
 * has been revealed. The object is empty if cards have not been dealt yet.
 *
 * The \e trick parameter is an array of position–card pairs, each containing a
 * position (field “position”) and a card (field “card”) played by the position
 * to the trick. The array is ordered in the order that the cards were played to
 * the trick. The array is empty if no cards have been played to the current
 * trick (possibly because the last trick was just completed or the playing
 * phase has not started).
 *
 * The \e tricksWon parameter is an object containing mapping from partnerships
 * to the number of tricks won by each side. If no tricks have been completed
 * (perhaps because bidding is not yet completed), both fields are zero.
 *
 * The \e vulnerability parameter is an object containing the current
 * vulnerabilities of the partnerships. If they are not known (perhaps because
 * there is no ongoing deal), the value is empty.
 *
 * \subsection bridgeprotocolcontroldeal deal
 *
 * - \b Command: deal
 * - \b Parameters:
 *   - game: UUID of the game
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
 *   - \e game: UUID of the game
 *   - \e player
 *   - \e call: see \ref jsoncall
 * - \b Reply: \e none
 *
 * All nodes use this command to make the specified call during the bidding.
 *
 * Peers MUST ignore the command and reply failure unless the node is allowed to
 * act for the player, the player has turn and the call is legal. If a
 * successful call command comes from a client, the peer MUST send the command
 * to other peers taking part in the game.
 *
 * The player argument MAY be omitted if the player the node is allowed to act
 * for is unique. The command SHOULD fail if a peer representing multiple
 * players omits the player argument.
 *
 * \subsection bridgeprotocolcontrolplay play
 *
 * - \b Command: play
 * - \b Parameters:
 *   - \e game: UUID of the game
 *   - \e player
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
 * act for the player, the player has turn and it is legal to play the
 * card. Declarer plays the hand of the dummy. If the play command comes from a
 * client, the peer MUST send the command to other peers. The index variant of
 * referring to the card MUST be used if the card is unknown to the peer.
 *
 * The player argument MAY be omitted if the player the node is allowed to act
 * for is unique. The command SHOULD fail if a peer representing multiple
 * players omits the player argument.
 *
 * \section bridgeprotocoleventcommands Events
 *
 * The peer SHOULD publish the following events through the event socket:
 *
 * \subsection bridgeprotocoleventdeal deal
 *
 * - \b Command: deal
 * - \b Parameters:
 *  - \e opener: the position of the opener of the bidding
 *  - \e vulnerability: the vulnerabilities in the deal
 *
 * This event is published whenever new cards are dealt.
 *
 * \subsection bridgeprotocoleventturn turn
 *
 * - \b Command: turn
 * - \b Parameters:
 *   - \e position: the position of the player to act
 *
 * This event is published whenever a new player gets turn. Declarer plays for
 * dummy during the playing phase.
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
 * - \b Parameters:
 *   - \e declarer: the declarer determined by the bidding
 *   - \e contract: the contract reached
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
 * - \b Parameters:
 *   - tricksWon: the number of tricks won by each partnership
 *   - score: the score awarded to the winner, see \ref jsonduplicatescore
 *
 * This event is published whenever a deal ends. If the deal passes out,
 * tricksWon are 0 for each partnership and score is null.
 */

#ifndef MAIN_COMMANDS_HH_
#define MAIN_COMMANDS_HH_

#include <string>

namespace Bridge {
namespace Main {

/** \brief See \ref bridgeprotocolcontrolbridgehlo
 */
inline constexpr auto HELLO_COMMAND = std::string_view {"bridgehlo"};

/** \brief See \ref bridgeprotocolcontrolbridgehlo
 */
inline constexpr auto VERSION_COMMAND = std::string_view {"version"};

/** \brief See \ref bridgeprotocolcontrolbridgehlo
 */
inline constexpr auto ROLE_COMMAND = std::string_view {"role"};

/** \brief See \ref bridgeprotocolcontrolgame
 */
inline constexpr auto GAME_COMMAND = std::string_view {"game"};

/** \brief See \ref bridgeprotocolcontroljoin
 */
inline constexpr auto JOIN_COMMAND = std::string_view {"join"};

/** \brief See \ref bridgeprotocolcontrolgame
 */
inline constexpr auto POSITIONS_COMMAND = std::string_view {"positions"};

/** \brief See \ref bridgeprotocolcontrolgame
 */
inline constexpr auto ARGS_COMMAND = std::string_view {"args"};

/** \brief See \ref bridgeprotocolcontrolgame
 */
inline constexpr auto CARD_SERVER_COMMAND = std::string_view {"cardServer"};

/** \brief See \ref bridgeprotocolcontrolgame
 */
inline constexpr auto ENDPOINT_COMMAND = std::string_view {"endpoint"};

/** \brief See \ref bridgeprotocolcontrolgame
 */
inline constexpr auto SERVER_KEY_COMMAND = std::string_view {"serverKey"};

/** \brief See \ref bridgeprotocolcontrolget
 */
inline constexpr auto GET_COMMAND = std::string_view {"get"};

/** \brief See \ref bridgeprotocolcontrolget
 */
inline constexpr auto PLAYER_COMMAND = std::string_view {"player"};

/** \brief See \ref bridgeprotocolcontrolget
 */
inline constexpr auto POSITION_COMMAND = std::string_view {"position"};

/** \brief See \ref bridgeprotocolcontrolget
 */
inline constexpr auto POSITION_IN_TURN_COMMAND = std::string_view {"positionInTurn"};

/** \brief See \ref bridgeprotocolcontrolget
 */
inline constexpr auto ALLOWED_CALLS_COMMAND = std::string_view {"allowedCalls"};

/** \brief See \ref bridgeprotocolcontrolget
 */
inline constexpr auto CALLS_COMMAND = std::string_view {"calls"};

/** \brief See \ref bridgeprotocolcontrolget
 */
inline constexpr auto DECLARER_COMMAND = std::string_view {"declarer"};

/** \brief See \ref bridgeprotocolcontrolget
 */
inline constexpr auto CONTRACT_COMMAND = std::string_view {"contract"};

/** \brief See \ref bridgeprotocolcontrolget
 */
inline constexpr auto ALLOWED_CARDS_COMMAND = std::string_view {"allowedCards"};

/** \brief See \ref bridgeprotocolcontrolget
 */
inline constexpr auto TRICKS_WON_COMMAND = std::string_view {"tricksWon"};

/** \brief See \ref bridgeprotocolcontrolget
 */
inline constexpr auto TRICK_COMMAND = std::string_view {"trick"};

/** \brief See \ref bridgeprotocolcontrolget
 */
inline constexpr auto VULNERABILITY_COMMAND = std::string_view {"vulnerability"};

/** \brief See \ref bridgeprotocolcontrolcall
 */
inline constexpr auto CALL_COMMAND = std::string_view {"call"};

/** \brief See \ref bridgeprotocolcontrolplay
 */
inline constexpr auto PLAY_COMMAND = std::string_view {"play"};

/** \brief See \ref bridgeprotocolcontrolplay
 */
inline constexpr auto CARD_COMMAND = std::string_view {"card"};

/** \brief See \ref bridgeprotocolcontrolplay
 */
inline constexpr auto INDEX_COMMAND = std::string_view {"index"};

/** \brief See \ref bridgeprotocolcontroldeal
 */
inline constexpr auto DEAL_COMMAND = std::string_view {"deal"};

/** \brief See \ref bridgeprotocolcontroldeal
 */
inline constexpr auto CARDS_COMMAND = std::string_view {"cards"};

/** \brief See \ref bridgeprotocoleventdeal
 */
inline constexpr auto OPENER_COMMAND = std::string_view {"opener"};

/** \brief See \ref bridgeprotocoleventturn
 */
inline constexpr auto TURN_COMMAND = std::string_view {"turn"};

/** \brief See \ref bridgeprotocoleventbidding
 */
inline constexpr auto BIDDING_COMMAND = std::string_view {"bidding"};

/** \brief See \ref bridgeprotocoleventdummy
 */
inline constexpr auto DUMMY_COMMAND = std::string_view {"dummy"};

/** \brief See \ref bridgeprotocoleventtrick
 */
inline constexpr auto WINNER_COMMAND = std::string_view {"winner"};

/** \brief See \ref bridgeprotocoleventdealend
 */
inline constexpr auto DEAL_END_COMMAND = std::string_view {"dealend"};

/** \brief See \ref bridgeprotocoleventdealend
 */
inline constexpr auto SCORE_COMMAND = std::string_view {"score"};

}
}

#endif // CARDSERVER_COMMANDS_HH_
