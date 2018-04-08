#include "main/NodePlayerControl.hh"

#include "bridge/BasicPlayer.hh"
#include "bridge/UuidGenerator.hh"
#include "Utility.hh"

#include <algorithm>
#include <cassert>
#include <vector>

namespace Bridge {
namespace Main {

class NodePlayerControl::Impl {
public:
    std::shared_ptr<Player> createPlayer(
        const std::string& node, const boost::optional<Uuid>& uuid);

    std::shared_ptr<const Player> getPlayer(
        const std::string& node, const boost::optional<Uuid>& uuid) const;

private:

    struct Element {
        std::string node;
        std::shared_ptr<Player> player;
    };

    // Assumption: access control happens per game and thus the vector contains
    // small number of elements. Searching the vector is fast.
    std::vector<Element> players;
    UuidGenerator uuidGenerator {createUuidGenerator()};
};

NodePlayerControl::NodePlayerControl() :
    impl {std::make_unique<Impl>()}
{
}

std::shared_ptr<Player> NodePlayerControl::Impl::createPlayer(
    const std::string& node, const boost::optional<Uuid>& uuid)
{
    const auto uuid_for_player = uuid ? *uuid : uuidGenerator();
    const auto iter = std::find_if(
        players.begin(), players.end(),
        [&uuid_for_player](const auto& e)
        {
            return dereference(e.player).getUuid() == uuid_for_player;
        });
    if (iter == players.end()) {
        const auto ret = std::make_shared<BasicPlayer>(uuid_for_player);
        players.emplace_back(Element {node, ret});
        return ret;
    }
    return nullptr;
}

std::shared_ptr<const Player> NodePlayerControl::Impl::getPlayer(
    const std::string& node, const boost::optional<Uuid>& uuid) const
{
    const auto last = players.end();
    if (uuid) {
        const auto iter = std::find_if(
            players.begin(), last,
            [&uuid](const auto& e)
            {
                return dereference(e.player).getUuid() == uuid;
            });
        if (iter != last && iter->node == node) {
            return iter->player;
        }
    } else {
        const auto func = [&node](const auto& e) { return e.node == node; };
        const auto iter = std::find_if(players.begin(), last, func);
        if (iter != last && std::find_if(iter + 1, last, func) == last) {
            return iter->player;
        }
    }
    return nullptr;
}

NodePlayerControl::~NodePlayerControl() = default;

std::shared_ptr<Player> NodePlayerControl::createPlayer(
    const std::string& node, const boost::optional<Uuid>& uuid)
{
    assert(impl);
    return impl->createPlayer(node, uuid);
}

std::shared_ptr<const Player> NodePlayerControl::getPlayer(
    const std::string& node, const boost::optional<Uuid>& uuid) const
{
    assert(impl);
    return impl->getPlayer(node, uuid);
}

}
}
