#include "bridge/BasicHand.hh"

#include <cassert>
#include <utility>

namespace Bridge {

void BasicHand::handleRequestReveal(IndexRange ns)
{
    notifyAll(CardRevealState::REQUESTED, ns);
}

}
