/** \file
 *
 * \brief Definition of Bridge::Main::PeerClientControl
 */

#ifndef MAIN_PEERCLIENTCONTROL_HH_
#define MAIN_PEERCLIENTCONTROL_HH_

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

/** \brief Class for interacting with peers and clients
 */
class PeerClientControl : private boost::noncopyable {
public:

    /** \brief Create new peer client control object
     *
     * \tparam PlayerIterator An input iterator that, when dereferenced,
     * returns a constant reference to Player object.
     *
     * \param first iterator to the first player controlled by the application
     * \param last iterator one past the last player controlled by the
     * application
     */
    template<typename PlayerIterator>
    PeerClientControl(PlayerIterator first, PlayerIterator last);

    /** \brief Add new client
     *
     * The client is mapped to one of the players controlled by this
     * application. Clients are assigned players to control in the order they
     * were in the range given as parameter to the constructor. If there are
     * no more players that the client could be assigned to, the client cannot
     * be added.
     *
     * \param identity identity string of the client
     *
     * \return pointer to the player the newly created client is allowed to
     * control, or nullptr if adding the client was not successful
     */
    const Player* addClient(std::string identity);

    /** \brief Add new peer
     *
     * Peer is other bridge application that controls some players that the
     * application itself or no other peer controls. If the peer tries to
     * assume control of a player either this application or another peer
     * added previously controls, the peer cannot be added.
     *
     * \tparam PlayerIterator An input iterator that, when dereferenced,
     * returns a reference to Player object.
     *
     * \param identity identity string of the peer
     * \param first iterator to the first player controlled by the application
     * \param last iterator one past the last player controlled by the
     * application
     *
     * \return true if adding the peer was successful, false otherwise
     */
    template<typename PlayerIterator>
    bool addPeer(
        std::string identity, PlayerIterator first, PlayerIterator last);

    /** \brief Get the unique player controlled by peer or client
     *
     * \param identity the identifier of the client or the peer
     *
     * \return pointer to a player uniquely controlled by a peer or a client
     * with \p identity. If \p identity is unrecognized or is a peer
     * controlling multiple players, nullptr is returned.
     */
    const Player* getPlayer(const std::string& identity);

    /** \brief Determine if a given peer or client controls a player
     *
     * \param identity the identifier of the client or peer
     * \param player the player that the peer or the client tries to control
     */
    bool isAllowedToAct(
        const std::string& identity, const Player& player) const;

    /** \brief Determine if the itself controls a player
     *
     * \param player the player
     *
     * \return true if the player belongs to the application itself, i.e. one
     * of clients is allowed to act for it
     */
    bool isSelfControlledPlayer(const Player& player) const;

    /** \brief Determine if all given players are controlled by a peer or self
     *
     * \tparam PlayerIterator A forward iterator that, when dereferenced,
     * returns a constant reference to a Player object.
     *
     * \param first iterator to the first player
     * \param last iterator one past the last player
     *
     * \return True if all players in the range are either self controlled of
     * controlled by a peer. It is not necessary that the self controlled
     * clients are already controlled by clients.
     */
    template<typename PlayerIterator>
    bool arePlayersControlled(PlayerIterator first, PlayerIterator last) const;

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

    using PeerClient = boost::variant<Peer, Client>;

    class GetPlayerVisitor;
    class IsAllowedToActVisitor;

    template<typename PlayerIterator>
    static auto makePlayerVector(PlayerIterator first, PlayerIterator last);

    bool internalAddPeer(std::string identity, PlayerVector players);

    std::map<std::string, PeerClient> others;
    std::size_t nClients {0u};
    PlayerVector allPlayers;
    const std::size_t nSelfPlayers;
};

template<typename PlayerIterator>
auto PeerClientControl::makePlayerVector(
    PlayerIterator first, PlayerIterator last)
{
    PlayerVector ret;
    for (; first != last; ++first) {
        ret.emplace_back(*first);
    }
    return ret;
}

template<typename PlayerIterator>
PeerClientControl::PeerClientControl(
    PlayerIterator first, PlayerIterator last) :
    allPlayers(makePlayerVector(first, last)),
    nSelfPlayers {allPlayers.size()}
{
}

template<typename PlayerIterator>
bool PeerClientControl::addPeer(
    std::string identity, PlayerIterator first, PlayerIterator last)
{
    return internalAddPeer(std::move(identity), PlayerVector(first, last));
}

template<typename PlayerIterator>
bool PeerClientControl::arePlayersControlled(
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

#endif // MAIN_PEERCLIENTCONTROL_HH_
