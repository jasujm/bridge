#include "bridge/BidIterator.hh"

namespace Bridge {

BidIterator::BidIterator(std::optional<Bid> bid) :
    bid {bid}
{
}

const Bid& BidIterator::dereference() const
{
    return *bid;
}

bool BidIterator::equal(const BidIterator& other) const
{
    return bid == other.bid;
}

void BidIterator::increment()
{
    bid = nextHigherBid(*bid);
}

}
