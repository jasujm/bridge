#include "cardserver/PeerEntry.hh"

#include "HexUtility.hh"

#include <tuple>
#include <utility>

namespace Bridge {
namespace CardServer {

PeerEntry::PeerEntry(
    Blob id, std::optional<std::string> endpoint,
    std::optional<Blob> serverKey) :
    id {std::move(id)},
    endpoint {std::move(endpoint)},
    serverKey {std::move(serverKey)}
{
}

bool operator==(const PeerEntry& lhs, const PeerEntry& rhs)
{
    return std::tie(lhs.id, lhs.endpoint, lhs.serverKey) ==
        std::tie(rhs.id, rhs.endpoint, rhs.serverKey);
}

std::ostream& operator<<(std::ostream& os, const PeerEntry& entry)
{
    os << formatHex(entry.id);
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
