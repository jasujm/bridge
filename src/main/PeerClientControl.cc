#include "main/PeerClientControl.hh"

#include "bridge/BridgeConstants.hh"
#include "Utility.hh"

#include <cassert>
#include <iterator>
#include <utility>

namespace Bridge {
namespace Main {

namespace {

template<typename Map, typename Key, typename Function>
auto callIfFound(Map&& map, const Key& key, Function&& func)
{
    const auto entry = map.find(key);
    if (entry != map.end()) {
        return func(entry->second);
    }
    return decltype(func(entry->second)) {};
}

}

class PeerClientControl::GetPlayerVisitor {
public:
    const Player* operator()(const Peer& peer) const
    {
        const auto& players = peer.players;
        if (players.size() == 1) {
            return &peer.players.front().get();
        }
        return nullptr;
    }

    const Player* operator()(const Client& client) const
    {
        return &client.player.get();
    }
};

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

const Player* PeerClientControl::getPlayer(const std::string& identity)
{
    return callIfFound(
        others, identity,
        [](const auto& peer_client) {
            return boost::apply_visitor(GetPlayerVisitor {}, peer_client);
        });
}

bool PeerClientControl::isAllowedToAct(
    const std::string& identity, const Player& player) const
{
    return callIfFound(
        others, identity,
        [&player](const auto& peer_client) {
            return boost::apply_visitor(
                IsAllowedToActVisitor {player}, peer_client);
        });
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
