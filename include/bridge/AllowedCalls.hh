/** \file
 *
 * \brief Definition of Bridge::getAllowedCalls
 */

#ifndef ALLOWEDCALLS_HH_
#define ALLOWEDCALLS_HH_

#include "bridge/Bidding.hh"
#include "bridge/BidIterator.hh"

namespace Bridge {

/** \brief Get allowed calls in the bidding
 *
 * Writes all allowed calls available for the next bidder in \p bidding to \p
 * out. No calls are allowed if the bidding has ended. If the bidding has not
 * ended, the allowed calls include pass, bids greater than the lowest allowed
 * bid in the bidding, double in case it is allowed and redouble in case it is
 * allowed.
 *
 * \param bidding the bidding
 * \param out the output iterator the Call objects are written to
 *
 * \return one past the position the last call was written to
 */
template<typename OutputIterator>
OutputIterator getAllowedCalls(const Bidding& bidding, OutputIterator out)
{
    if (bidding.hasEnded()) {
        return out;
    }
    *out++ = Pass {};
    if (bidding.isDoublingAllowed()) {
        *out++ = Double {};
    }
    if (bidding.isRedoublingAllowed()) {
        *out++ = Redouble {};
    }
    return std::copy(
        BidIterator(bidding.getLowestAllowedBid()), BidIterator(std::nullopt),
        out);
}

}

#endif // ALLOWEDCALLS_HH_
