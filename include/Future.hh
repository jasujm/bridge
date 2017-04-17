/** \file
 *
 * \brief Utilities expected to be implemented in future C++ standards
 *
 * This header contains custom implementations of classes expected to be
 * implemented later in standard C++. When standard implementation is available,
 * uses of the functions defined in this header should be replaced with standard
 * functions.
 */

#ifndef FUTURE_HH_
#define FUTURE_HH_

#include <initializer_list>

namespace Bridge {

/** \brief Implementation of future std::data
 *
 * \param c container
 *
 * \return c.data()
 */
template <typename C>
constexpr decltype(auto) data(C& c)
{
    return c.data();
}

/** \brief Implementation of future std::data
 *
 * \param c container
 *
 * \return c.data()
 */
template <typename C>
constexpr decltype(auto) data(const C& c)
{
    return c.data();
}

/** \brief Implementation of future std::data
 *
 * \param array C style array
 *
 * \return array
 */
template <typename T, std::size_t N>
constexpr T* data(T (&array)[N]) noexcept
{
    return array;
}

/** \brief Implementation of future std::data
 *
 * \param il initializer list
 *
 * \return il.begin()
 */
template <typename E>
constexpr const E* data(std::initializer_list<E> il) noexcept
{
    return il.begin();
}

/** \brief Implementation of future std::size
 *
 * \param c container
 *
 * \return c.size()
 */
template <typename C>
constexpr decltype(auto) size(const C& c)
{
    return c.size();
}

/** \brief Implementation of future std::size
 *
 * \param array C style array
 *
 * \return size of the array
 */
template <typename T, std::size_t N>
constexpr std::size_t size(const T (&array)[N]) noexcept
{
    return N;
}

}

#endif // ENDIF_HH_
