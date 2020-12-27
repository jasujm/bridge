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
