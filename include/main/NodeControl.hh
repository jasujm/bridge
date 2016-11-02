/** \file
 *
 * \brief Definition of Bridge::Main::NodeControl
 */

#ifndef MAIN_NODECONTROL_HH_
#define MAIN_NODECONTROL_HH_

#include <boost/core/noncopyable.hpp>
#include <boost/variant/variant.hpp>

#include <algorithm>
#include <functional>
#include <map>
#include <string>
#include <utility>

namespace Bridge {

class Player;

namespace Main {

/** \brief Utility class for access control of nodes
 *
 * NodeControl can be used to establish relationship between clients and peers
 * (collectively known as nodes), and the players they control or
 * represent. Each peer (including the application itself) may \e represent
 * multiple players. Each client may \e control one of the players that the
 * application represents.
 */
class NodeControl : private boost::noncopyable {
public:

    /** \brief Create new node control object
     *
     * \tparam PlayerIterator An input iterator that, when dereferenced,
     * returns a constant reference to Player object.
     *
     * \param first iterator to the first player represented by the application
     * \param last iterator one past the last player represented by the
     * application
     */
    template<typename PlayerIterator>
    NodeControl(PlayerIterator first, PlayerIterator last);

    /** \brief Add new client
     *
     * This method maps a client to one of the players represented by this
     * application. The method assigns one player for the client to control in
     * the order they were in the range given as parameter to the
     * constructor. If there are no more players that could be assigned, no
     * client may be added.
     *
     * If \p identity is the identity of an already added client, the player
     * already assigned to the client is returned.
     *
     * \param identity identity of the client
     *
     * \return pointer to the player the newly created or existing client is
     * allowed to control, or nullptr if adding the client was not successful
     */
    const Player* addClient(const std::string& identity);

    /** \brief Add new peer
     *
     * A peer is another bridge application that represents some players that
     * the application itself or no other peer represents. If the peer requests
     * representation of a player either this application or another peer added
     * represents, the peer cannot be added.
     *
     * \tparam PlayerIterator An input iterator that, when dereferenced,
     * returns a reference to Player object.
     *
     * \param identity identity of the peer
     * \param first iterator to the first player represented by the peer
     * \param last iterator one past the last player represented by the
     * peer
     *
     * \return true if adding the peer was successful, false otherwise
     */
    template<typename PlayerIterator>
    bool addPeer(
        std::string identity, PlayerIterator first, PlayerIterator last);

    /** \brief Get the unique player controlled by the given node
     *
     * \param identity identity of the node
     *
     * \return pointer to a player uniquely controlled by the node with \p
     * identity. If \p identity is unrecognized or is a peer representing
     * multiple players, nullptr is returned.
     */
    const Player* getPlayer(const std::string& identity) const;

    /** \brief Determine if a given peer or client is allowed to act for the
     * given player
     *
     * \param identity identity of the node
     * \param player the player supposedly controlled or represented by the node
     *
     * \return true if the node with identity is allowed to act for the player,
     * i.e. either is a peer representing the player or a client to whom the
     * control of the player is assigned to
     */
    bool isAllowedToAct(
        const std::string& identity, const Player& player) const;

    /** \brief Determine if the application itself represents a player
     *
     * \param player the player
     *
     * \return true if the player is represented by \e this application itself,
     * false otherwise
     */
    bool isSelfRepresentedPlayer(const Player& player) const;

    /** \brief Determine if all given players are represented
     *
     * \tparam PlayerIterator A forward iterator that, when dereferenced,
     * returns a constant reference to a Player object.
     *
     * \param first iterator to the first player
     * \param last iterator one past the last player
     *
     * \return true if all players in the range are represented either by self
     * or another client, false otherwise. It is not necessary that the self
     * represented players are already controlled by a client.
     */
    template<typename PlayerIterator>
    bool arePlayersRepresented(PlayerIterator first, PlayerIterator last) const;

private:

    using PlayerVector = std::vector<std::reference_wrapper<const Player>>;

    struct Peer {
        explicit Peer(PlayerVector players);
        PlayerVector players;
    };

    struct Client {
        explicit Client(const Player& player);
        std::reference_wrapper<const Player> player;
    };

    using Node = boost::variant<Peer, Client>;

    class GetPlayerVisitor;
    class IsAllowedToActVisitor;

    template<typename PlayerIterator>
    static auto makePlayerVector(PlayerIterator first, PlayerIterator last);

    bool internalAddPeer(std::string identity, PlayerVector players);

    std::map<std::string, Node> others;
    std::size_t nClients {0u};
    PlayerVector allPlayers;
    const std::size_t nSelfPlayers;
};

template<typename PlayerIterator>
auto NodeControl::makePlayerVector(
    PlayerIterator first, PlayerIterator last)
{
    PlayerVector ret;
    for (; first != last; ++first) {
        ret.emplace_back(*first);
    }
    return ret;
}

template<typename PlayerIterator>
NodeControl::NodeControl(
    PlayerIterator first, PlayerIterator last) :
    allPlayers(makePlayerVector(first, last)),
    nSelfPlayers {allPlayers.size()}
{
}

template<typename PlayerIterator>
bool NodeControl::addPeer(
    std::string identity, PlayerIterator first, PlayerIterator last)
{
    return internalAddPeer(std::move(identity), PlayerVector(first, last));
}

template<typename PlayerIterator>
bool NodeControl::arePlayersRepresented(
    PlayerIterator first, PlayerIterator last) const
{
    return std::is_permutation(
        first, last,
        allPlayers.begin(), allPlayers.end(),
        [](const Player& player1, const Player& player2)
        {
            return &player1 == &player2;
        });
}

}
}

#endif // MAIN_NODECONTROL_HH_
