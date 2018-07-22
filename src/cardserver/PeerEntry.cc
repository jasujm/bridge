#include "cardserver/PeerEntry.hh"

#include "Logging.hh" // TODO: hex encoding to some other header

#include <tuple>
#include <utility>

namespace Bridge {
namespace CardServer {

PeerEntry::PeerEntry(
    Messaging::Identity identity, std::optional<std::string> endpoint) :
    identity {std::move(identity)},
    endpoint {std::move(endpoint)}
{
}

bool operator==(const PeerEntry& lhs, const PeerEntry& rhs)
{
    return std::tie(lhs.identity, lhs.endpoint) ==
        std::tie(rhs.identity, rhs.endpoint);
}

std::ostream& operator<<(std::ostream& os, const PeerEntry& entry)
{
    os << asHex(entry.identity);
    if (entry.endpoint) {
        os << " " << *entry.endpoint;
    }
    return os;
}

}
}
