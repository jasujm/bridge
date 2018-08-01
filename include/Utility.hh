/** \file
 *
 * \brief Definition of general purpose utilities
 *
 * Although the utilities in this do not depend on any other classes or
 * functions inside the Bridge namespace, the functions are still inside the
 * namespace to avoid name conflicts.
 */

#ifndef UTILITY_HH_
#define UTILITY_HH_

#include <boost/range/irange.hpp>
#include <boost/iterator/transform_iterator.hpp>

#include <cstddef>
#include <memory>
#include <stdexcept>

namespace Bridge {

/** \brief Check if i < n
 *
 * \param i the index to check
 * \param n the upper bound
 *
 * \return i, if i < n
 *
 * \throw std::out_of_range, if i >= n
 */
template<typename Integer1, typename Integer2>
auto checkIndex(Integer1 i, Integer2 n)
{
    if (i >= n) {
        throw std::out_of_range("Index out of range");
    }
    return i;
}

/** \brief Check if pointer (or pointer‐like object) is dereferencalbe, and
 * dereference it
 *
 * \param p pointer to be referenced
 *
 * \return reference to whatever p points to
 *
 * \throw std::invalid_argument, if p is null
 */
template<typename T>
decltype(auto) dereference(const T& p)
{
    if (!p) {
        throw std::invalid_argument("Trying to dereference nullptr");
    }
    return *p;
}

/** \brief Convert pointer‐like object to pointer
 *
 * \param p pointer‐like object
 *
 * \return pointer to the object obtained by derefencing \p p, or nullptr if \p
 * p is empty
 */
template<typename T>
auto getPtr(const T& p) -> decltype(std::addressof(*p))
{
    if (p) {
        return std::addressof(*p);
    }
    return nullptr;
}

/** \brief Generate functor that will compare addresses of two references
 *
 * \note The functor borrows a reference to the parameter. It is the
 * responsibility of the client to ensure that the reference given as
 * parameter won’t become dangling during the lifetime of the functor.
 *
 * \param t object addresses are compared to
 *
 * \return functor whose operator()(const T&) returns true if its parameter
 * has the same address as t
 */
template<typename T>
auto compareAddress(const T& t)
{
    return [&t](const T& t2)
    {
        return std::addressof(t) == std::addressof(t2);
    };
}

/** \brief Create an iterator that accesses container using indices from
 * another iterator
 *
 * \param iter the underlying iterator for accessing indices
 * \param container the container accessed
 *
 * \return an iterator that will access the members of the underlying
 * container
 */
template<typename IndexIterator, typename ContainerType>
auto containerAccessIterator(IndexIterator iter, ContainerType& container)
{
    return boost::make_transform_iterator(
        iter,
        [&container](const auto n) -> decltype(auto)
        {
            return container.at(n);
        });
}

/** \brief Range over integers
 *
 * Generate range over integers from \p m to \p n (exclusive). This can be
 * used in ranged for
 *
 * \code{.cc}
 * for (const auto i : from_to(271, 314)) {
 *     std::cout << i << std::endl;
 * }
 * \endcode
 *
 * The type of the generated integers is that of the upper bound. The lower
 * bound must be convertible to the type of the upper bound with non‐narrowing
 * conversion. When the lower bound is zero, there is shorthand \ref to.
 *
 * \param m the lower bound
 * \param n the upper bound
 *
 * \return A range from \p m to \p n (exclusive upper bound)
 *
 * \sa to()
 */
template<typename Integer1, typename Integer2>
auto from_to(Integer1 m, Integer2 n)
{
    return boost::irange(Integer2 {m}, n);
}

/** \brief Shorthand for from_to(0, n)
 *
 * \param n the upper bound of the range
 *
 * \return A range from 0 to \p n (exclusive upper bound)
 *
 * \sa from_to()
 */
template<typename Integer>
auto to(Integer n)
{
    return from_to(Integer {}, n);
}

}

#endif // UTILITY_HH_
