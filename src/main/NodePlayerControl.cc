#include "main/NodePlayerControl.hh"

#include "bridge/BasicPlayer.hh"
#include "bridge/UuidGenerator.hh"
#include "Utility.hh"

#include <cassert>
#include <map>

namespace Bridge {
namespace Main {

using Messaging::Identity;

class NodePlayerControl::Impl {
public:
    std::shared_ptr<Player> getOrCreatePlayer(
        const Identity& node, const std::optional<Uuid>& uuid);

private:

    std::multimap<Identity, std::shared_ptr<Player>> nodes;
    std::map<Uuid, std::pair<Identity, std::shared_ptr<Player>>> players;
    UuidGenerator uuidGenerator {createUuidGenerator()};
};

NodePlayerControl::NodePlayerControl() :
    impl {std::make_unique<Impl>()}
{
}

std::shared_ptr<Player> NodePlayerControl::Impl::getOrCreatePlayer(
    const Identity& node, const std::optional<Uuid>& uuid)
{
    Uuid uuid_for_player;
    if (!uuid) {
        const auto iter = nodes.lower_bound(node);
        if (iter == nodes.end()) {
            uuid_for_player = uuidGenerator();
        } else {
            const auto next_iter = std::next(iter);
            if (next_iter == nodes.end() || next_iter->first != node) {
                return iter->second;
            } else {
                return nullptr;
            }
        }
    } else {
        uuid_for_player = *uuid;
    }
    const auto iter = players.find(uuid_for_player);
    if (iter == players.end()) {
        const auto ret = std::make_shared<BasicPlayer>(uuid_for_player);
        players.emplace(uuid_for_player, std::pair {node, ret});
        nodes.emplace(node, ret);
        return ret;
    } else if (iter->second.first == node) {
        return iter->second.second;
    }
    return nullptr;
}

NodePlayerControl::~NodePlayerControl() = default;

std::shared_ptr<Player> NodePlayerControl::getOrCreatePlayer(
    const Identity& node, const std::optional<Uuid>& uuid)
{
    assert(impl);
    return impl->getOrCreatePlayer(node, uuid);
}

}
}
