/** \file
 *
 * \brief Definition of utilities used for determining cards owned by positions
 */

#ifndef CARDSFORPOSITION_HH_
#define CARDSFORPOSITION_HH_

#include "bridge/Position.hh"

#include <algorithm>
#include <iterator>
#include <cstddef>

namespace Bridge {

/** \brief Determine the indices of cards dealt to given position
 *
 * \param position the position
 *
 * \return Range containing the 13 indices owned by the given position. The
 * indices are the following:
 *   - North owns cards 0–12
 *   - East owns cards 13–25
 *   - South owns cards 26–38
 *   - West owns cards 39–51
 *
 * \throw std::invalid_argument if \p position is invalid
 */
inline auto cardsFor(Position position)
{
    const auto n = positionOrder(position);
    return boost::irange(n * N_CARDS_PER_PLAYER, (n + 1) * N_CARDS_PER_PLAYER);
}

/** \brief Determine the indices of cards dealt to a range of positions
 *
 * \tparam PositionIterator An input iterator that, when dereferenced, returns
 * a value convertible to Position.
 *
 * \param first iterator to the first position in the range
 * \param last iterator one past the last position in the range
 *
 * \return Union of the indices obtained by calling cardsFor(Position) with
 * each position in the range.
 *
 * \throw std::invalid_argument if any position in the range is invalid
 */
template<typename PositionIterator>
auto cardsFor(PositionIterator first, PositionIterator last)
{
    auto ret = std::vector<std::size_t> {};
    for (; first != last; ++first) {
        const auto ns = cardsFor(*first);
        ret.reserve(ret.size() + ns.size());
        std::copy(ns.begin(), ns.end(), std::back_inserter(ret));
    }
    return ret;
}

}

#endif // CARDSFORPOSITION_HH_
