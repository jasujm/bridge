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
#include <boost/optional/optional.hpp>

#include <vector>

namespace Bridge {

/** \brief Concrete implementation of the Bidding interface
 *
 * BasicBidding maintains turns based on predetermined list of players. The
 * first player in the list is the opener, after which the bidding proceeds
 * clockwise.
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

    std::size_t handleGetNumberOfCalls() const override;

    Position handleGetOpeningPosition() const override;

    Call handleGetCall(std::size_t n) const override;

    bool handleIsCallAllowed(const Call& call) const override;

    Contract handleGetContract() const override;

    Position handleGetDeclarerPosition() const override;

    bool handleHasEnded() const override;

    bool handleHasContract() const override;

    const Position openingPosition;
    std::vector<Call> calls;
    boost::logic::tribool lastBidderHasTurn;
    boost::optional<Contract> contract;
};

}

#endif // BASICBIDDING_HH_
