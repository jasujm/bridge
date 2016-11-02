#include "main/NodeControl.hh"

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

class NodeControl::GetPlayerVisitor {
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

class NodeControl::IsAllowedToActVisitor {
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

const Player* NodeControl::addClient(const std::string& identity)
{
    if (const auto player = getPlayer(identity)) {
        return player;
    } else if (nClients >= nSelfPlayers) {
        return nullptr;
    }

    assert(nClients < allPlayers.size());
    const auto& player = allPlayers[nClients].get();
    const auto entry = others.emplace(identity, Client {player});
    if (entry.second) {
        ++nClients;
        return &player;
    }
    return nullptr;
}

const Player* NodeControl::getPlayer(const std::string& identity) const
{
    return callIfFound(
        others, identity,
        [](const auto& peer_client) {
            return boost::apply_visitor(GetPlayerVisitor {}, peer_client);
        });
}

bool NodeControl::isAllowedToAct(
    const std::string& identity, const Player& player) const
{
    return callIfFound(
        others, identity,
        [&player](const auto& peer_client) {
            return boost::apply_visitor(
                IsAllowedToActVisitor {player}, peer_client);
        });
}

bool NodeControl::isSelfRepresentedPlayer(const Player& player) const
{
    const auto first = allPlayers.begin();
    const auto last = std::next(first, nSelfPlayers);
    return std::find_if(first, last, compareAddress(player)) != last;
}

bool NodeControl::internalAddPeer(
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

NodeControl::Peer::Peer(PlayerVector players) :
    players(std::move(players))
{
}

NodeControl::Client::Client(const Player& player) :
    player {player}
{
}

}
}
