#include "bridge/Player.hh"

namespace Bridge {

Player::~Player() = default;

Uuid Player::getUuid() const
{
    return handleGetUuid();
}

}
