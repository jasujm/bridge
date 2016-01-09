/** \file
 *
 * \brief Definition of an utility to iterate several containers at once
 */

#ifndef ZIP_HH_
#define ZIP_HH_

#include <boost/iterator/zip_iterator.hpp>

#include <algorithm>
#include <iterator>
#include <stdexcept>
#include <tuple>

namespace Bridge {

/** \brief Wrapper for zipping several ranges into a range iterating each
 *
 * A ZipRange object can be used in range‐based for loop to simultaneously
 * iterate several underlying ranges. ZipRange can capture each underlying
 * range either by reference or by value depending on the template
 * arguments. A ZipRange object is intended to be created using zip() function
 * which can deduce the correct types.
 *
 * \tparam Ranges the types of the underlying ranges
 *
 * \sa zip()
 */
template<typename... Ranges>
class ZipRange
{
    static_assert(sizeof...(Ranges) > 0, "At least one range required");

public:

    /**
     * \brief Create new zip range
     *
     * \param ranges the underlying ranges
     *
     * \throw std::invalid_argument unless all ranges have the same size
     */
    explicit ZipRange(Ranges&&... ranges) :
        ranges {
            internalCheckRangesHaveEqualSize(std::forward<Ranges>(ranges)...)}
    {
    }

    /** \brief Iterator to the beginning of the range
     *
     * \return zip iterator to tuple containing the elements from the
     * beginning of the underlying ranges
     */
    auto begin() const
    {
        return internalMakeZipIterator(
            [](const auto& range)
            {
                return std::begin(range);
            }, Is {});
    }

    /** \brief Iterator to the end of the range
     *
     * \return zip iterator to tuple containing the elements from the
     * end of the underlying ranges
     */
    auto end() const
    {
        return internalMakeZipIterator(
            [](const auto& range)
            {
                return std::end(range);
            }, Is {});
    }

private:

    using Is = std::index_sequence_for<Ranges...>;

    static auto internalCheckRangesHaveEqualSize(Ranges&&... ranges)
    {
        const auto sizes = {
            std::distance(std::begin(ranges), std::end(ranges))...};
        const auto first_size = sizes.begin();
        const auto n = *first_size;
        if (
            !std::all_of(
                std::next(first_size), sizes.end(),
                [n](const auto m) { return m == n; })) {
            throw std::invalid_argument("Ranges are not equal length");
        }
        return std::forward_as_tuple(std::forward<Ranges>(ranges)...);
    }

    template<typename F, std::size_t... I>
    auto internalMakeZipIterator(F f, std::index_sequence<I...>) const
    {
        return boost::make_zip_iterator(
            boost::make_tuple(f(std::get<I>(ranges))...));
    }

    std::tuple<Ranges...> ranges;
};

/** \brief Simultaneously access values from multiple ranges
 *
 * This function is inspired by the zip function in Python. It is used to
 * simultaneously access elements from multiple ranges. The underlying
 * iterator is zip_iterator from the Boost iterator library (see
 * http://www.boost.org/doc/libs/1_60_0/libs/iterator/doc/index.html for
 * documentation).
 *
 * \code{.cc}
 * std::vector<std::string> names { "one", "euler", "pi" };
 * double numbers[] { 1.0, 2.71, 3.14 };
 * for (const auto t : zip(names, numbers)) {
 *     std::cout << t.get<0>() << ": " << t.get<1>() << std::endl;
 * }
 * \endcode
 *
 * std::begin() and std::end() must return valid iterators for the
 * ranges. Before returning this function checks that the ranges of each
 * iterator pair has the same length. This may make the function inefficient
 * for iterators that are not random access iterators.
 *
 * \param ranges the ranges (at least one) to be iterated
 *
 * \return ZipRange for iterating the underlying \p ranges
 *
 * \throw std::invalid_argument unless all ranges have the same size
 */
template<typename... Ranges>
auto zip(Ranges&&... ranges)
{
    return ZipRange<Ranges...> {std::forward<Ranges>(ranges)...};
}

}

#endif // ZIP_HH_
