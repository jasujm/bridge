/** \file
 *
 * \brief Definition of fundamental bridge constants needed by several classes
 */

#ifndef BRIDGECONSTANTS_HH_
#define BRIDGECONSTANTS_HH_

/** \brief Top level namespace of the Bridge framework
 *
 * The Bridge namespace directly contains classes related to fundamental
 * concepts of contract bridge. It also contains several subnamespaces for
 * clearly identifiable collections of higher level functionality.
 */
namespace Bridge {

/** \brief Number of players in bridge game
 */
constexpr auto N_PLAYERS = 4;

/** \brief Number of cards in playing card deck
 */
constexpr auto N_CARDS = 52;

/** \brief Number of cards per player (hand size) in bridge game
 */
constexpr auto N_CARDS_PER_PLAYER = N_CARDS / N_PLAYERS; // 13

}

#endif // BRIDGECONSTANTS_HH_
