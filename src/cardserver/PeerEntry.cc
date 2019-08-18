#include "cardserver/PeerEntry.hh"

#include "HexUtility.hh"

#include <tuple>
#include <utility>

namespace Bridge {
namespace CardServer {

PeerEntry::PeerEntry(
    std::string endpoint,
    std::optional<Blob> serverKey) :
    endpoint {std::move(endpoint)},
    serverKey {std::move(serverKey)}
{
}

bool operator==(const PeerEntry& lhs, const PeerEntry& rhs)
{
    return std::tie(lhs.endpoint, lhs.serverKey) ==
        std::tie(rhs.endpoint, rhs.serverKey);
}

std::ostream& operator<<(std::ostream& os, const PeerEntry& entry)
{
    os << entry.endpoint;
    if (entry.serverKey) {
        os << " " << formatHex(*entry.serverKey);
    }
    return os;
}

}
}
