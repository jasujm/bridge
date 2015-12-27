/** \file
 *
 * \brief Definition of Bridge::BasicHand class
 */

#ifndef BASICHAND_HH_
#define BASICHAND_HH_

#include "bridge/Hand.hh"

#include <boost/core/noncopyable.hpp>

#include <functional>
#include <vector>

namespace Bridge {

/** \brief Concrete implementation of the Hand interface
 */
class BasicHand : public Hand, private boost::noncopyable {
public:

    /** \brief Create new hand
     *
     * \tparam CardIterator an input iterator which must return a
     * reference to a Card object when dereferenced
     *
     * \param first the first card to be dealt
     * \param last the last card to be dealt
     */
    template<typename CardIterator>
    BasicHand(CardIterator first, CardIterator last);

private:

    struct CardEntry {
        std::reference_wrapper<const Card> card;
        bool isPlayed;

        CardEntry(const Card& card);
    };

    void handleMarkPlayed(std::size_t n) override;

    const Card& handleGetCard(std::size_t n) const override;

    bool handleIsPlayed(std::size_t n) const override;

    std::size_t handleGetNumberOfCards() const override;

    std::vector<CardEntry> cards;
};

template<typename CardIterator>
BasicHand::BasicHand(CardIterator first, CardIterator last) :
    cards(first, last)
{
}

}

#endif // BASICHAND_HH_
