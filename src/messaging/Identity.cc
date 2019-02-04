#include "messaging/Identity.hh"

#include "messaging/MessageUtility.hh"
#include "HexUtility.hh"

#include <utility>
#include <tuple>

namespace Bridge {
namespace Messaging {

Identity::Identity(UserId userId, RoutingId routingId) :
    userId {std::move(userId)},
    routingId {std::move(routingId)}
{
}

Identity identityFromMessage(
    zmq::message_t& message, zmq::message_t* routerIdentityFrame)
{
    const auto* user_id = "";
    try {
        user_id = message.gets("User-Id");
    } catch (const zmq::error_t&) {
        // metadata property not found, ignore
    }
    auto routing_id_view = routerIdentityFrame ?
        messageView(*routerIdentityFrame) :
        decltype(messageView(*routerIdentityFrame)) {};
    return {
        user_id, RoutingId(routing_id_view.begin(), routing_id_view.end()) };
}

bool operator==(const Identity& lhs, const Identity& rhs)
{
    return std::tie(lhs.userId, lhs.routingId) ==
        std::tie(rhs.userId, rhs.routingId);
}

bool operator<(const Identity& lhs, const Identity& rhs)
{
    return std::tie(lhs.userId, lhs.routingId) <
        std::tie(rhs.userId, rhs.routingId);
}

std::ostream& operator<<(std::ostream& os, const Identity& identity)
{
    return os << identity.userId << "/" << formatHex(identity.routingId);
}

}
}
