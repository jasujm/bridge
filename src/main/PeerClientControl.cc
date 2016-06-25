#include "main/PeerClientControl.hh"

#include "bridge/BridgeConstants.hh"
#include "Utility.hh"

#include <cassert>
#include <iterator>
#include <utility>

namespace Bridge {
namespace Main {

class PeerClientControl::IsAllowedToActVisitor {
public:
    IsAllowedToActVisitor(const Player& player) :
        player {player}
    {
    }

    bool operator()(const Peer& peer) const
    {
        const auto& players = peer.players;
        return std::find_if(
            players.begin(), players.end(), compareAddress(player))
            != players.end();
    }

    bool operator()(const Client& client) const
    {
        return &client.player.get() == &player;
    }

private:

    const Player& player;
};

const Player* PeerClientControl::addClient(std::string identity)
{
    if (nClients >= nSelfPlayers)
    {
        return nullptr;
    }

    assert(nClients < allPlayers.size());
    const auto& player = allPlayers[nClients].get();
    const auto entry = others.emplace(std::move(identity), Client {player});
    if (entry.second) {
        ++nClients;
        return &player;
    }
    return nullptr;
}

bool PeerClientControl::isAllowedToAct(
    const std::string& identity, const Player& player) const
{
    const auto entry = others.find(identity);
    if (entry != others.end()) {
        return boost::apply_visitor(
            IsAllowedToActVisitor {player}, entry->second);
    }
    return false;
}

bool PeerClientControl::isSelfControlledPlayer(const Player& player) const
{
    const auto first = allPlayers.begin();
    const auto last = std::next(first, nSelfPlayers);
    return std::find_if(first, last, compareAddress(player)) != last;
}

bool PeerClientControl::internalAddPeer(
    std::string identity, PlayerVector players)
{
    for (const auto& player : allPlayers) {
        if (
            std::find_if(
                players.begin(), players.end(), compareAddress(player.get()))
            != players.end())
        {
            return false;
        }
    }

    const auto n = allPlayers.size();
    std::copy(players.begin(), players.end(), std::back_inserter(allPlayers));
    try {
        const auto entry = others.emplace(
            std::move(identity), Peer {std::move(players)});
        return entry.second;
    } catch (...) {
        allPlayers.erase(std::next(allPlayers.begin(), n), allPlayers.end());
        throw;
    }
}

PeerClientControl::Peer::Peer(PlayerVector players) :
    players(std::move(players))
{
}

PeerClientControl::Client::Client(const Player& player) :
    player {player}
{
}

}
}
