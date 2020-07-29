/** \file
 *
 * \brief Definition of Bridge::BasicBidding class
 */

#ifndef BASICBIDDING_HH_
#define BASICBIDDING_HH_

#include "bridge/Bidding.hh"
#include "bridge/Contract.hh"

#include <boost/core/noncopyable.hpp>
#include <boost/logic/tribool.hpp>

#include <optional>
#include <vector>

namespace Bridge {

/** \brief Concrete implementation of the Bidding interface
 *
 * BasicBidding stores the calls of the players. The first player in
 * the list is the opener, after which the bidding proceeds clockwise.
 */
class BasicBidding : public Bidding, private boost::noncopyable {
public:

    /** \brief Create new bidding
     *
     * \param openingPosition the position of the opener
     */
     BasicBidding(Position openingPosition);

private:

    void handleAddCall(const Call& call) override;

    int handleGetNumberOfCalls() const override;

    Position handleGetOpeningPosition() const override;

    Call handleGetCall(int n) const override;

    const Position openingPosition;
    std::vector<Call> calls;
};

}

#endif // BASICBIDDING_HH_
