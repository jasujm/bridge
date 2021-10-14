#include "messaging/Identity.hh"

#include "messaging/MessageUtility.hh"
#include "HexUtility.hh"

#include <utility>

namespace Bridge {
namespace Messaging {

Identity::Identity(UserId userId, RoutingId routingId) :
    userId {std::move(userId)},
    routingId {std::move(routingId)}
{
}
void setSocketRoutingId(Socket& socket, RoutingId routingId)
{
    socket.set(zmq::sockopt::routing_id, messageBuffer(routingId));
}

Identity identityFromMessage(Message& message, Message* routerIdentityFrame)
{
    const auto* user_id = "";
    try {
        user_id = message.gets("User-Id");
    } catch (const SocketError&) {
        // metadata property not found, ignore
    }
    auto routing_id_view = routerIdentityFrame ?
        messageView(*routerIdentityFrame) :
        decltype(messageView(*routerIdentityFrame)) {};
    return {
        user_id, RoutingId(routing_id_view.begin(), routing_id_view.end()) };
}

std::ostream& operator<<(std::ostream& os, const Identity& identity)
{
    return os << identity.userId << "/" << formatHex(identity.routingId);
}

}
}
