/** \file
 *
 * \brief Definition of an utility to iterate index and contents of a container
 */

#ifndef ENUMERATE_HH_
#define ENUMERATE_HH_

#include <boost/iterator/iterator_adaptor.hpp>

#include <iterator>
#include <utility>

namespace Bridge {

/** \brief The value type for EnumerateIterator
 *
 * This is a std::pair containing an index as its first and whatever is
 * returned by dereferencing the underlying iterator as its second element.
 *
 * \sa EnumerateIterator
 */
template<typename Iterator>
using EnumerateIteratorValue =
    std::pair<typename std::iterator_traits<Iterator>::difference_type,
              typename std::iterator_traits<Iterator>::reference>;

/** \brief Iterator for simultaneously accessing position and value
 *
 * EnumerateIterator is an iterator that is used to simultaneously access the
 * index and value of an underlying iterator. The reference type of
 * EnumerateIterator is a pair consisting of index and whatever is returned when
 * dereferencing the underlying iterator.  More specifically, index is the
 * number of times the iterator has been incremented. If the iterator is
 * decremented beyond its initial position (possible e.g. if an iterator not
 * pointing to the beginning of a range is wrapped), the index may also be
 * negative.
 *
 * \tparam Iterator the type of the underlying iterator
 *
 * \sa makeEnumerateIterator()
 */
template<typename Iterator>
class EnumerateIterator : public boost::iterator_adaptor<
    EnumerateIterator<Iterator>,
    Iterator,
    EnumerateIteratorValue<Iterator>,
    boost::use_default,
    EnumerateIteratorValue<Iterator>>
{
public:

    /** \brief Create new enumerate iterator
     *
     * \param iter the underlying iterator
     */
    explicit EnumerateIterator(const Iterator& iter) :
        base_ {iter},
        index {}
    {
    }

private:

    using base_ = typename EnumerateIterator::iterator_adaptor_;

    typename base_::reference dereference() const
    {
        return {this->index, *this->base()};
    }

    void advance(typename base_::difference_type n)
    {
        this->index += n;
        this->base_reference() += n;
    }

    void increment()
    {
        ++this->index;
        ++this->base_reference();
    }

    void decrement()
    {
        --this->index;
        --this->base_reference();
    }

    typename base_::difference_type index;
    friend class boost::iterator_core_access;
};

/** \brief Make enumerate iterator
 *
 * Helper for wrapping an iterator to EnumerateIterator
 *
 * \param iter the iterator to be wrapped
 *
 * \return EnumerateIterator object wrapping \p iter
 */
template<typename Iterator>
auto makeEnumerateIterator(const Iterator& iter)
{
    return EnumerateIterator<Iterator> {iter};
}

/** \brief Enumerable range
 *
 * An EnumerateRange object can be used in range‐based for loop to
 * simultaneously iterate index and value of an underlying
 * range. EnumerateRange can capture the underlying range either by reference
 * or by value depending on the template argument type. An EnumerateRange
 * object is intended to be created using the enumerate() function which can
 * deduce the correct type.
 *
 * \tparam Range the type of the underlying range
 *
 * \sa enumerate()
 */
template<typename Range>
class EnumerateRange
{
public:

    /** \brief Create new enumerate range
     *
     * \param range the underlying range
     */
    explicit EnumerateRange(Range&& range) :
        range(std::forward<Range>(range))
    {
    }

    /** \brief The beginning of the range
     *
     * \return EnumerateIterator to the first element of the underlying range
     */
    auto begin()
    {
        return makeEnumerateIterator(std::begin(range));
    }

    /** \brief The end of the range
     *
     * \return EnumerateIterator to the last element of the underlying range
     */
    auto end()
    {
        return makeEnumerateIterator(std::end(range));
    }

private:

    Range range;
};

/** \brief Simultaneously iterate index and value of a range
 *
 * This function is inspired by the enumerate function in Python. It is used to
 * simultaneously access the position and the value of an underlying range in a
 * range‐based for loop. The elements are std::pair objects containing index as
 * the first and the element from the underlying range as the second element.
 *
 * Example printing the elements of a vector as an ordered list:
 *
 * \code{.cc}
 * int array[] { 111, 222, 333 };
 * for (auto&& e : enumerate(array)) {
 *     std::cout << e.first + 1 << ". " << e.second << std::endl;
 * }
 * \endcode
 *
 * \param range The range to iterate over. It is required that when std::begin()
 * and std::end() are applied to \p range, a valid iterator is returned.
 *
 * \return EnumerateRange for iterating index and value of the underlying \p
 * range
 */
template<typename Range>
auto enumerate(Range&& range)
{
    return EnumerateRange<Range> {std::forward<Range>(range)};
}

}

#endif // ENUMERATE_HH_
