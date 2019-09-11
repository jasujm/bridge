/** \file
 *
 * \brief Definition of Bridge::Bidding interface
 */

#ifndef BIDDING_HH_
#define BIDDING_HH_

#include "bridge/Call.hh"
#include "bridge/Contract.hh"
#include "bridge/Position.hh"

#include <boost/iterator/counting_iterator.hpp>
#include <boost/iterator/transform_iterator.hpp>
#include <boost/logic/tribool_fwd.hpp>

#include <optional>
#include <utility>

namespace Bridge {

/** \brief State machine for bridge bidding
 */
class Bidding
{
public:

    virtual ~Bidding();

    /** \brief Make call
     *
     * The call if successful if the bidding is ongoing, it is made by the
     * position that has the turn and the call is allowed according to
     * contract bridge rules. Call is always unsuccessful if the bidding has
     * ended.
     *
     * \param position the position of the player making the call
     * \param call the call to be made
     *
     * \return true if the call was successful, false otherwise
     */
    bool call(Position position, const Call& call);

    /** \brief Determine the lowest allowed bid
     *
     * \return the lowest Bid that the position in turn can make, or none if
     * hasEnded() == true or no higher bids are allowed
     */
    std::optional<Bid> getLowestAllowedBid() const;

    /** \brief Determine if doubling is allowed
     *
     * \return true if hasEnded() == false and the player in turn can double,
     * false otherwise
     */
    bool isDoublingAllowed() const;

    /** \brief Determine if redoubling is allowed
     *
     * \return true if hasEnded() == false and the player in turn can
     * redouble, false otherwise
     */
    bool isRedoublingAllowed() const;

    /** \brief Determine which position has the turn to bid
     *
     * \return the position in turn, or none if the bidding has ended
     */
    std::optional<Position> getPositionInTurn() const;

    /** \brief Determine the number of calls made in the bidding
     *
     * \return the number of calls made in the bidding
     */
    int getNumberOfCalls() const;

    /** \brief Determine which position is the opener
     *
     * The opener is the first position to bid.
     *
     * \return the opener of this bidding
     */
    Position getOpeningPosition() const;

    /** \brief Retrieve a call in the bidding sequence
     *
     * \param n the index of the call to retrieve
     *
     * \return nth call made by given position
     *
     * \throw std::out_of_range if n >= getNumberOfCalls()
     */
    Call getCall(int n) const;

    /** \brief Determine if there is contract
     *
     * \return true if the bidding has ended and there is contract, false if
     * the bidding has ended but it was passed out, indeterminate otherwise
     */
    boost::logic::tribool hasContract() const;

    /** \brief Determine contract.
     *
     * The outer optional is none if bidding is still ongoing. The inner
     * optional is none if bidding has ended but it didn’t end up in contract
     * (i.e. no bids were made).
     *
     * \return optional contract determined by the bidding, or none if bidding
     * is still ongoing
     */
    std::optional<std::optional<Contract>> getContract() const;

    /** \brief Determine the declarer
     *
     * The declarer is the position that leads to the first trick. The outer
     * optional is none if bidding is still ongoing. The inner optional is
     * none if bidding has ended but it didn’t end up in contract (i.e. no
     * bids were made).
     *
     * \return the declarer, or none if the bidding is still ongoing
     */
    std::optional<std::optional<Position>> getDeclarerPosition() const;

    /** \brief Determine if the bidding has ended
     *
     * \return true if the bidding has ended, false otherwise
     */
    bool hasEnded() const;

    /** \brief Get iterator to the beginning of calls
     *
     * \sa biddingCallIterator()
     */
    auto begin() const;

    /** \brief Get iterator to the end of calls
     *
     * \sa biddingCallIterator()
     */
    auto end() const;

private:

    /** \brief Handle for adding a call to the bidding
     *
     * It may be assumed that hasEnded() == false and
     * handleIsCallAllowed(call) == true.
     */
    virtual void handleAddCall(const Call& call) = 0;

    /** \brief Handle for determining number of calls made in the bidding
      *
      * \sa getNumberOfCalls()
      */
    virtual int handleGetNumberOfCalls() const = 0;

    /** \brief Handle for determining the opening position
     *
     * \sa getOpeningPosition()
     */
    virtual Position handleGetOpeningPosition() const = 0;

    /** \brief Handle for determining the nth call made in this bidding
     *
     * It may be asumed that n < handleGetNumberOfCalls().
     *
     * \param n the index of the call to retrieve
     *
     * \return the call corresponding to n
     */
    virtual Call handleGetCall(int n) const = 0;

    /** \brief Handle for determining whether call is allowed
     *
     * It may be assumed that hasEnded() == false.
     *
     * \return true if call is allowed, false otherwise
     */
    virtual bool handleIsCallAllowed(const Call& call) const = 0;

    /** \brief Handle for determining the lowest allowed bid
     *
     * It may be assumed that hasEnded() == false.
     *
     * \return The lowest bid that the position in turn can make
     */
    virtual std::optional<Bid> handleGetLowestAllowedBid() const = 0;

    /** \brief Handle for determining which contract was determined by the
     * bidding
     *
     * It may be assumed that hasEnded() == true and handleHasContract() ==
     * true.
     *
     * \sa getContract()
     */
    virtual Contract handleGetContract() const = 0;

    /** \brief Handle for determining at which position the declarer is
     *
     * It may be assumed that hasEnded() == true and handleHasContract() ==
     * true.
     *
     * \sa getDeclarerPosition()
     */
    virtual Position handleGetDeclarerPosition() const = 0;

    /** \brief Handle for determining whether or not the bidding has ended
     *
     * \sa hasEnded()
     */
    virtual bool handleHasEnded() const = 0;

    /** \brief  Handle for determining whether or not the bidding ended up in a
     * contract.
     *
     * The bidding does not end up in a contract if the bidding starts with
     * four consecutive passes. It may be assumed that hasEnded() == true.
     */
    virtual bool handleHasContract() const = 0;

    template<class T>
    std::optional<std::optional<T>> internalGetIfHasContract(
        T (Bidding::*memfn)() const) const;
};

/** \brief Create iterator for iterating over calls in bidding
 *
 * \param bidding the bidding from which the calls are retrieved
 * \param n the index where iterating the calls begins
 *
 * \return Iterator that, when deferenced, returns a pair containing the
 * following:
 *   - Position who made call \p n
 *   - The call made
 */
inline auto biddingCallIterator(const Bidding& bidding, int n)
{
    return boost::make_transform_iterator(
        boost::make_counting_iterator(n),
        [&bidding](const auto i)
        {
            const auto position = clockwise(bidding.getOpeningPosition(), i);
            return std::make_pair(position, bidding.getCall(i));
        });
}

inline auto Bidding::begin() const
{
    return biddingCallIterator(*this, 0);
}

inline auto Bidding::end() const
{
    return biddingCallIterator(*this, getNumberOfCalls());
}

}

#endif // BIDDING_HH_
