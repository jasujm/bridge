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

#include <boost/iterator/transform_iterator.hpp>

#include <cstddef>
#include <memory>
#include <stdexcept>
#include <ranges>
#include <type_traits>

namespace Bridge {

/** \brief Check if 0 < i < n
 *
 * \param i the index to check
 * \param n the upper bound
 *
 * \return i, if 0 < i < n
 *
 * \throw std::out_of_range, if i < 0 || i >= n
 */
template<typename Integer1, typename Integer2>
constexpr auto checkIndex(Integer1 i, Integer2 n)
{
    if (i < 0 || i >= n) {
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
constexpr decltype(auto) dereference(const T& p)
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
constexpr auto getPtr(T& p) -> decltype(std::addressof(*p))
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
 * Generate an increasing range over integers from \p m to \p n
 * (exclusive). This can be used in ranged for
 *
 * \code{.cc}
 * for (const auto i : from_to(271, 314)) {
 *     std::cout << i << std::endl;
 * }
 * \endcode
 *
 * \param m the lower bound
 * \param n the upper bound
 *
 * \return A range from \p m to \p n (exclusive upper bound)
 *
 * \throw std::invalid_argument if \p m > \p n
 *
 * \sa to()
 */
template<std::integral Integer>
constexpr auto from_to(std::type_identity_t<Integer> m, Integer n)
{
    if (m > n) {
        throw std::invalid_argument {"Invalid integer range"};
    }
    return std::ranges::views::iota(m, n);
}

/** \brief Shorthand for from_to(0, n)
 *
 * \param n the upper bound of the range
 *
 * \return A range from 0 to \p n (exclusive upper bound)
 *
 * \throw std::invalid_argument if \p n < 0
 *
 * \sa from_to()
 */
template<std::integral Integer>
constexpr auto to(Integer n)
{
    return from_to(Integer {}, n);
}

/** \brief Strided range over integers
 *
 * Generate an increasing range over integers from \p m to \p n (exclusive) with
 * step size \p step.
 *
 * \param m the lower bound
 * \param n the upper bound
 * \param step the step size
 *
 * \return A range from \p m to \p n (exclusive upper bound)
 *
 * \throw std::invalid_argument if \p m > \p n or \p step <= 0
 *
 * \sa to()
 */
template<std::integral Integer>
constexpr auto from_to(
    std::type_identity_t<Integer> m, Integer n,
    std::type_identity_t<Integer> step)
{
    if (step <= 0) {
        throw std::invalid_argument {"Invalid step"};
    }
    return to((n - m) / step) | std::ranges::views::transform(
        [m, step](const auto i) { return m + i * step; });
}

}

#endif // UTILITY_HH_
