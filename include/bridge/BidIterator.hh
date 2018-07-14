/** \file
 *
 * \brief Definition of iterator for iterating bids
 */

#ifndef BIDITERATOR_HH_
#define BIDITERATOR_HH_

#include "bridge/Bid.hh"

#include <boost/iterator/iterator_facade.hpp>

#include <optional>

namespace Bridge {

/** \brief Iterator for iterating bids
 *
 * BidIterator is an input iterator used to iterate consecutive bids in
 * bidding order.
 *
 * Example for creating a vector containing all possible bids:
 *
 * \code{.cc}
 * std::vector<Bid>(BidIterator(Bid::LOWEST_BID), BidIterator(std::nullopt))
 * \endcode
 *
 * \sa Bid
 */
class BidIterator : public boost::iterator_facade<
    BidIterator,
    const Bid,
    boost::incrementable_traversal_tag> {
public:

    /** \brief Create new bid iterator
     *
     * \param bid the first bid in the range, or none if this iterator
     * represents the iterator past the highest bid
     */
    BidIterator(std::optional<Bid> bid);

private:

    const Bid& dereference() const;
    bool equal(const BidIterator& other) const;
    void increment();

    std::optional<Bid> bid;
    friend class boost::iterator_core_access;
};

}

#endif // BIDITERATOR_HH_
