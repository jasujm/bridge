#include "main/NodePlayerControl.hh"

#include "bridge/BasicPlayer.hh"
#include "bridge/UuidGenerator.hh"
#include "Utility.hh"

#include <cassert>
#include <map>

namespace Bridge {
namespace Main {

using Messaging::Identity;

struct UserIdCompare {
    bool operator()(const Identity& lhs, const Identity& rhs) const {
        return lhs.userId < rhs.userId;
    }
};

bool equalIdentity(const Identity& lhs, const Identity& rhs)
{
    const auto compare = UserIdCompare {};
    return !compare(lhs, rhs) && !compare(rhs, lhs);
}

class NodePlayerControl::Impl {
public:
    std::shared_ptr<Player> getOrCreatePlayer(
        const Identity& node, const Uuid& uuid);

private:

    std::multimap<Identity, std::shared_ptr<Player>, UserIdCompare> nodes;
    std::map<Uuid, std::pair<Identity, std::shared_ptr<Player>>> players;
    UuidGenerator uuidGenerator {createUuidGenerator()};
};

NodePlayerControl::NodePlayerControl() :
    impl {std::make_unique<Impl>()}
{
}

std::shared_ptr<Player> NodePlayerControl::Impl::getOrCreatePlayer(
    const Identity& node, const Uuid& uuid)
{
    const auto player_iter = players.find(uuid);
    if (player_iter == players.end()) {
        const auto ret = std::make_shared<BasicPlayer>(uuid);
        players.emplace(uuid, std::pair {node, ret});
        nodes.emplace(node, ret);
        return ret;
    } else if (equalIdentity(player_iter->second.first, node)) {
        return player_iter->second.second;
    }
    return nullptr;
}

NodePlayerControl::~NodePlayerControl() = default;

std::shared_ptr<Player> NodePlayerControl::getOrCreatePlayer(
    const Identity& node, const Uuid& uuid)
{
    assert(impl);
    return impl->getOrCreatePlayer(node, uuid);
}

}
}
