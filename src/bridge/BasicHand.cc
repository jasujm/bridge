#include "bridge/BasicHand.hh"

namespace Bridge {

void BasicHand::handleRequestReveal(const IndexVector& ns)
{
    notifyAll(CardRevealState::REQUESTED, ns);
}

}
