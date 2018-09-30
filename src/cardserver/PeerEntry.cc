#include "cardserver/PeerEntry.hh"

#include "HexUtility.hh"

#include <tuple>
#include <utility>

namespace Bridge {
namespace CardServer {

PeerEntry::PeerEntry(
    Messaging::Identity identity, std::optional<std::string> endpoint,
    std::optional<Blob> serverKey) :
    identity {std::move(identity)},
    endpoint {std::move(endpoint)},
    serverKey {std::move(serverKey)}
{
}

bool operator==(const PeerEntry& lhs, const PeerEntry& rhs)
{
    return std::tie(lhs.identity, lhs.endpoint, lhs.serverKey) ==
        std::tie(rhs.identity, rhs.endpoint, rhs.serverKey);
}

std::ostream& operator<<(std::ostream& os, const PeerEntry& entry)
{
    os << formatHex(entry.identity);
    if (entry.endpoint) {
        os << " " << *entry.endpoint;
    }
    if (entry.serverKey) {
        os << " " << formatHex(*entry.serverKey);
    }
    return os;
}

}
}
