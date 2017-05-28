#include "bridge/BasicPlayer.hh"

namespace Bridge {

BasicPlayer::BasicPlayer(const Uuid& uuid) :
    uuid {uuid}
{
}

Uuid BasicPlayer::handleGetUuid() const
{
    return uuid;
}

}  // Bridge
