#include "main/NodePlayerControl.hh"

#include "bridge/BasicPlayer.hh"
#include "bridge/UuidGenerator.hh"
#include "main/BridgeGameRecorder.hh"
#include "Utility.hh"

#include <cassert>
#include <map>

namespace Bridge {
namespace Main {

using Messaging::UserId;

class NodePlayerControl::Impl {
public:
    std::shared_ptr<Player> getOrCreatePlayer(
        const UserId& node, const Uuid& uuid,
        BridgeGameRecorder* recorder);

private:

    std::map<Uuid, std::pair<UserId, std::shared_ptr<Player>>> players;
    UuidGenerator uuidGenerator {createUuidGenerator()};
};

NodePlayerControl::NodePlayerControl() :
    impl {std::make_unique<Impl>()}
{
}

std::shared_ptr<Player> NodePlayerControl::Impl::getOrCreatePlayer(
    const UserId& node, const Uuid& uuid, BridgeGameRecorder* recorder)
{
    const auto player_iter = players.find(uuid);
    if (player_iter == players.end()) {
        auto record_player = false;
        if (recorder) {
            if (const auto user_id = recorder->recallPlayer(uuid)) {
                if (*user_id != node) {
                    return nullptr;
                }
            } else {
                record_player = true;
            }
        }
        const auto ret = std::make_shared<BasicPlayer>(uuid);
        players.emplace(uuid, std::pair {node, ret});
        if (record_player) {
            assert(recorder);
            recorder->recordPlayer(uuid, node);
        }
        return ret;
    } else if (player_iter->second.first == node) {
        return player_iter->second.second;
    }
    return nullptr;
}

NodePlayerControl::~NodePlayerControl() = default;

std::shared_ptr<Player> NodePlayerControl::getOrCreatePlayer(
    const UserId& node, const Uuid& uuid, BridgeGameRecorder* recorder)
{
    assert(impl);
    return impl->getOrCreatePlayer(node, uuid, recorder);
}

}
}
